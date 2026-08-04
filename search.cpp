// search.cpp - Implementación de la búsqueda
#include "search.h"
#include <iostream>
#include <chrono>
#include <algorithm>
#include <atomic>
#include <cstring>

namespace hy3 {

bool insufficient_material(const Board& b) {
    int n[2][6] = {0};
    int total = 0;
    for (int sq = 0; sq < 64; sq++) {
        Piece p = b.squares[sq];
        if (piece_type(p) == NONE) continue;
        n[piece_color(p)][piece_type(p)]++;
        total++;
    }
    if (total <= 2) return true;                 // K vs K
    if (total == 3) {                            // K + (N o B) vs K
        if (n[0][KNIGHT] + n[0][BISHOP] + n[1][KNIGHT] + n[1][BISHOP] == 1)
            return true;
    }
    return false;
}

// ---------------- Transposition table ----------------
// Estructura de 2 cubos por índice:
//   Cubo 0 (depth-preferred): conserva la entrada con mayor profundidad.
//   Cubo 1 (always-replace):  acepta siempre la última entrada evaluada.
// El número de índices es potencia de 2 (se enmascara con TT_MASK).
struct TTEntry {
    uint64_t key   = 0;
    int16_t  depth = 0;
    int16_t  score = 0;
    uint32_t move  = 0;
    uint8_t  flag  = 0;    // 0 exact, 1 lower, 2 upper
};
struct TTBucket { TTEntry e[2]; };

static std::vector<TTBucket> tt;
static size_t TT_MASK = 0;
static int    g_tt_mb = 64;      // tamaño por defecto (MB)

static void tt_alloc(int mb) {
    if (mb < 1) mb = 1;
    if (mb > 4096) mb = 4096;
    g_tt_mb = mb;
    // nº de buckets = potencia de 2 que quepa en 'mb' megabytes.
    size_t bytes = (size_t)mb * 1024 * 1024;
    size_t nbuckets = bytes / sizeof(TTBucket);
    size_t pow2 = 1;
    while (pow2 * 2 <= nbuckets) pow2 *= 2;
    if (pow2 < 1024) pow2 = 1024;
    tt.assign(pow2, TTBucket{});
    TT_MASK = pow2 - 1;
}

static void tt_init() {
    if (!tt.empty()) return;
    tt_alloc(g_tt_mb);
}

void tt_resize(int mb) { tt_alloc(mb); }

static inline size_t tt_idx(uint64_t h) { return (size_t)h & TT_MASK; }

void tt_clear() {
    if (tt.empty()) return;
    std::fill(tt.begin(), tt.end(), TTBucket{});
}

// Sonda: devuelve puntero a la entrada que coincide con 'h', o nullptr.
static inline TTEntry* tt_probe(uint64_t h) {
    TTBucket& b = tt[tt_idx(h)];
    if (b.e[0].key == h) return &b.e[0];
    if (b.e[1].key == h) return &b.e[1];
    return nullptr;
}

// Almacena una entrada aplicando la política de 2 cubos.
static inline void tt_store(uint64_t h, int depth, int score, uint32_t move, uint8_t flag) {
    TTBucket& b = tt[tt_idx(h)];
    // Cubo 0: depth-preferred. Reemplaza si es la misma posición o si la nueva
    // búsqueda es al menos igual de profunda que la almacenada.
    if (b.e[0].key == h || depth >= b.e[0].depth) {
        // Conserva la jugada previa si la nueva no trae una (p. ej. cota de NMP).
        if (move == 0 && b.e[0].key == h) move = b.e[0].move;
        b.e[0].key = h; b.e[0].depth = (int16_t)depth;
        b.e[0].score = (int16_t)score; b.e[0].move = move; b.e[0].flag = flag;
    } else {
        // Cubo 1: always-replace.
        b.e[1].key = h; b.e[1].depth = (int16_t)depth;
        b.e[1].score = (int16_t)score; b.e[1].move = move; b.e[1].flag = flag;
    }
}

// Historial de claves Zobrist de la partida real (para triple repetición).
static std::vector<uint64_t> g_game_hist;
void set_game_history(const std::vector<uint64_t>& keys) { g_game_hist = keys; }

// Normalización de puntuaciones de mate al guardar/leer de la TT.
// En el árbol las puntuaciones de mate son relativas a la raíz (MATE - ply).
// En la TT deben almacenarse relativas al nodo (MATE - distancia_al_mate)
// para que sean válidas al recuperarse desde otro ply.
static inline int score_to_tt(int sc, int ply) {
    if (sc >  MATE - 1000) return sc + ply;
    if (sc < -MATE + 1000) return sc - ply;
    return sc;
}
static inline int score_from_tt(int sc, int ply) {
    if (sc >  MATE - 1000) return sc - ply;
    if (sc < -MATE + 1000) return sc + ply;
    return sc;
}

// ---------------- Contexto de búsqueda ----------------
struct Context {
    uint64_t nodes = 0;
    int64_t start_ms = 0;
    int64_t time_ms = 0;
    bool stop = false;
    std::vector<uint64_t> rep;                   // hashes en el camino actual
    Move killers[128][2];
    // History heuristic: historial[color][from][to] (piezas no-captura)
    int history[2][64][64];
    Move parent_move = 0;   // jugada que condujo a este nodo (para anti-repetir)
    bool has_time_limit = true;
    // Tabla triangular de la Variante Principal (PV)
    Move pv_table[128][128];
    int pv_len[128];
};

static int64_t now_ms() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
}

static std::atomic<bool> g_stop_flag{false};

static bool time_up(Context& ctx) {
    if (ctx.stop) return true;
    if (g_stop_flag.load(std::memory_order_relaxed)) { ctx.stop = true; return true; }
    if (!ctx.has_time_limit) return false;
    if (now_ms() - ctx.start_ms >= ctx.time_ms) { ctx.stop = true; return true; }
    return false;
}

// ---------------- Ordenación de movimientos ----------------
static int move_order_score(const Board& b, Move m, Move tt_move, const Context& ctx, int ply) {
    if (tt_move != 0 && m == tt_move) return 100000000;
    int flag = move_flag(m);
    if (flag == FLAG_CAPTURE || flag == FLAG_EP) {
        Square to = move_to(m);
        Piece victim = b.squares[to];
        int vv = (piece_type(victim) == NONE) ? 100 : PIECE_VALUE[piece_type(victim)];
        int av = PIECE_VALUE[piece_type(b.squares[move_from(m)])];
        // MVV-LVA: prioriza capturar piezas caras con piezas baratas.
        // Bonifica además las capturas que sobreviven al intercambio (SEE >= 0).
        int see_score = b.see(to, b.side);
        int base = 1000000 + vv * 16 - av;
        if (see_score >= 0) base += 500000;   // captura "ganadora" o neutral
        return base;
    }
    int promo = move_promo(m);
    // promo (1=N,2=B,3=R,4=Q) coincide con PieceType, indexar sin desplazar.
    if (promo != 0) return 900000 + PIECE_VALUE[promo] * 16;
    // killers
    if (ply < 128) {
        if (ctx.killers[ply][0] == m) return 800000;
        if (ctx.killers[ply][1] == m) return 700000;
    }
    // history heuristic (movimientos no-captura)
    Square f = move_from(m), t = move_to(m);
    int score = (f >= 0 && f < 64 && t >= 0 && t < 64) ? ctx.history[b.side][f][t] : 0;
    // Anti-repetir: penaliza deshacer la jugada que nos trajo aquí (ping-pong).
    if (ctx.parent_move != 0) {
        Square pf = move_from(ctx.parent_move), pt = move_to(ctx.parent_move);
        if (f == pt && t == pf) score -= 50000;
    }
    return score;
}

static void order_moves(const Board& b, MoveList& moves, Move tt_move, const Context& ctx, int ply) {
    // Precomputa la puntuación de cada jugada una sola vez (evita recalcular
    // SEE y history en cada comparación del sort, que es O(n log n)).
    int score[256];
    int n = moves.count;
    for (int i = 0; i < n; i++)
        score[i] = move_order_score(b, moves[i], tt_move, ctx, ply);
    // Ordenación por selección sobre índices (n pequeño; evita asignaciones).
    for (int i = 0; i < n - 1; i++) {
        int best = i;
        for (int j = i + 1; j < n; j++)
            if (score[j] > score[best]) best = j;
        if (best != i) {
            std::swap(score[i], score[best]);
            Move t = moves[i]; moves[i] = moves[best]; moves[best] = t;
        }
    }
}

static const int INF = MATE + 1000;
static const int MAX_PLY = 128;

// ---------------- Quiescence ----------------
static Score quiescence(Board& b, Score alpha, Score beta, int ply, Context& ctx) {
    if (time_up(ctx)) return 0;
    if (ply >= MAX_PLY - 1) {
        ctx.pv_len[MAX_PLY - 1] = 0;
        return evaluate(b);
    }
    ctx.nodes++;
    ctx.pv_len[ply] = 0;
    uint64_t h = b.zkey;
    for (uint64_t prev : ctx.rep) if (prev == h) return 0;
    if (b.halfmove >= 100) return 0;

    bool in_check = b.in_check();
    if (in_check) {
        // En jaque: considerar todas las evasiones (no solo capturas)
        MoveList moves; b.legal_moves(moves);
        if (moves.empty()) return -MATE + ply;
        order_moves(b, moves, 0, ctx, ply);
        Score best = -INF;
        ctx.rep.push_back(h);
        for (int i = 0; i < moves.count; i++) {
            Move m = moves[i];
            Undo u; b.make_move(m, u);
            Score sc = -quiescence(b, -beta, -alpha, ply + 1, ctx);
            b.unmake_move(m, u);
            if (ctx.stop) { ctx.rep.pop_back(); return alpha; }
            if (sc > best) best = sc;
            if (sc > alpha) alpha = sc;
            if (alpha >= beta) break;
        }
        ctx.rep.pop_back();
        return best;
    }

    Score stand = evaluate(b);
    if (stand >= beta) return beta;
    if (stand > alpha) alpha = stand;

    MoveList moves; b.legal_moves(moves);
    // Solo capturas y promociones en quiescence
    MoveList qm;
    for (int i = 0; i < moves.count; i++) {
        Move m = moves[i];
        if (move_flag(m) == FLAG_CAPTURE || move_flag(m) == FLAG_EP || move_promo(m) != 0)
            qm.push_back(m);
    }
    order_moves(b, qm, 0, ctx, ply);
    ctx.rep.push_back(h);
    for (int i = 0; i < qm.count; i++) {
        Move m = qm[i];
        Undo u; b.make_move(m, u);
        Score sc = -quiescence(b, -beta, -alpha, ply + 1, ctx);
        b.unmake_move(m, u);
        if (ctx.stop) { ctx.rep.pop_back(); return alpha; }
        if (sc >= beta) { ctx.rep.pop_back(); return beta; }
        if (sc > alpha) alpha = sc;
    }
    ctx.rep.pop_back();
    return alpha;
}

// ---------------- Negamax con PVS ----------------
static Score negamax(Board& b, int depth, Score alpha, Score beta, int ply, Context& ctx) {
    if (time_up(ctx)) return 0;
    if (ply >= MAX_PLY - 1) {
        ctx.pv_len[MAX_PLY - 1] = 0;
        return evaluate(b);
    }
    ctx.nodes++;
    ctx.pv_len[ply] = 0;
    uint64_t h = b.zkey;
    // Repetición dentro del árbol de búsqueda -> tablas.
    for (uint64_t prev : ctx.rep) if (prev == h) return 0;
    // Repetición contra el historial real de la partida (una coincidencia en la
    // línea actual + una en la partida ya basta para forzar el reclamo de tablas
    // y evitar cegueras ante repeticiones previas al inicio de la búsqueda).
    if (ply > 0) {
        for (uint64_t prev : g_game_hist) if (prev == h) return 0;
    }
    if (b.halfmove >= 100) return 0;                        // regla de 50
    if (insufficient_material(b)) return 0;

    bool in_check = b.in_check();
    if (depth <= 0) return quiescence(b, alpha, beta, ply, ctx);

    // Sonda de la TT (estructura de 2 cubos)
    Move ttm = 0;
    bool is_pv = (beta - alpha) > 1;
    TTEntry* tte = tt_probe(h);
    if (tte) {
        ttm = (Move)tte->move;
        if (tte->depth >= depth && !is_pv) {
            int sc = score_from_tt(tte->score, ply);
            if (tte->flag == 0) return sc;
            else if (tte->flag == 1 && sc > alpha) alpha = sc;
            else if (tte->flag == 2 && sc < beta) beta = sc;
            if (alpha >= beta) return sc;
        }
    }

    // Evaluación estática (para podas basadas en el margen). Solo se calcula
    // cuando no estamos en jaque, ya que en jaque no es fiable.
    Score static_eval = in_check ? 0 : evaluate(b);

    // ---- Reverse Futility Pruning / Static Null-Move Pruning ----
    // En profundidades bajas, si la evaluación estática supera beta por un
    // margen amplio y no estamos en jaque ni cerca de un mate, podamos.
    if (!is_pv && !in_check && depth <= 3 &&
        beta > -MATE + 1000 && beta < MATE - 1000) {
        int margin = 120 * depth;
        if (static_eval - margin >= beta) return static_eval - margin;
    }

    MoveList moves; b.legal_moves(moves);
    if (moves.empty()) {
        if (in_check) return -MATE + ply;
        return 0;
    }
    order_moves(b, moves, ttm, ctx, ply);

    Score alpha_orig = alpha;
    Score best = -INF;
    Move best_move = 0;
    bool first = true;

    // Null-Move Pruning: si no estamos en jaque (ni el rival), no es final de
    // juego (pocas piezas) y tenemos margen de ventana, probamos un "null move".
    // Si el rival no puede refutar con profundidad reducida, podamos la rama.
    if (!in_check && !b.in_check(Color(b.side ^ 1)) && depth >= 3) {
        int nonPawn = 0;
        for (int sq = 0; sq < 64; sq++) {
            Piece p = b.squares[sq];
            if (piece_type(p) == NONE || piece_color(p) != b.side) continue;
            int t = piece_type(p);
            if (t == PAWN || t == KING) continue;
            nonPawn++;
        }
        if (nonPawn >= 2) {
            int R = 2 + (depth / 4);
            // Reducimos la ventana (null-window) alrededor de beta.
            ctx.rep.push_back(h);
            Square saved_ep = b.ep_square;
            uint64_t saved_key = b.zkey;
            b.ep_square = NO_SQ;
            b.side = Color(b.side ^ 1);  // null move (cambio de lado)
            b.zkey = b.hash();           // clave correcta de la posición tras el null-move
            Score null_sc = -negamax(b, depth - 1 - R, -beta, -beta + 1, ply + 1, ctx);
            b.side = Color(b.side ^ 1);  // restaurar lado
            b.ep_square = saved_ep;
            b.zkey = saved_key;
            ctx.rep.pop_back();
            if (ctx.stop) return (best == -INF) ? 0 : best;
            if (null_sc >= beta) {
                // El rival no refuta: podamos. Guardamos en TT como cota.
                if (!ctx.stop)
                    tt_store(h, depth, score_to_tt(beta, ply), 0, 1);
                return beta;
            }
        }
    }

    ctx.rep.push_back(h);
    int move_idx = 0;
    for (int mi = 0; mi < moves.count; mi++) {
        Move m = moves[mi];
        bool is_capture = (move_flag(m) == FLAG_CAPTURE || move_flag(m) == FLAG_EP);
        bool is_quiet = !is_capture && move_promo(m) == 0;

        // ---- Late Move Pruning (LMP) ----
        // En profundidades bajas, tras probar suficientes jugadas ordenadas y
        // sin estar en jaque, descartamos las jugadas silenciosas restantes.
        if (!is_pv && !in_check && is_quiet && depth <= 4 && best > -MATE + 1000) {
            int limit = 3 + depth * depth;   // d1=4, d2=7, d3=12, d4=19
            if (move_idx >= limit) { move_idx++; continue; }
        }

        // ---- Futility Pruning ----
        // En profundidad baja, si la evaluación estática más un margen no alcanza
        // alpha, las jugadas silenciosas difícilmente lo mejorarán: se podan.
        if (!is_pv && !in_check && is_quiet && depth <= 3 && !first &&
            best > -MATE + 1000) {
            int fmargin = 100 + 100 * depth;
            if (static_eval + fmargin <= alpha) { move_idx++; continue; }
        }

        Undo u; b.make_move(m, u);
        ctx.parent_move = m;
        // Extensión por jaque: solo extendemos el movimiento que DA jaque,
        // nunca la evasión.
        bool gives_check = b.in_check();
        int mext = gives_check ? 1 : 0;
        Score sc;
        if (first) {
            sc = -negamax(b, depth - 1 + mext, -beta, -alpha, ply + 1, ctx);
        } else {
            // Late Move Reductions: reduce pliegues de jugadas no-PV, no
            // captura y que no dan jaque, tras los primeros movimientos.
            int reduction = 0;
            if (depth >= 3 && !in_check && move_flag(m) == FLAG_NORMAL && !gives_check) {
                reduction = 1;
                if (move_idx > 4) reduction = 2;   // más reducción tras varias jugadas
            }
            if (reduction > 0) {
                sc = -negamax(b, depth - 1 - reduction + mext, -alpha - 1, -alpha, ply + 1, ctx);
                if (sc > alpha && sc < beta)
                    sc = -negamax(b, depth - 1 + mext, -beta, -alpha, ply + 1, ctx);
            } else {
                sc = -negamax(b, depth - 1 + mext, -alpha - 1, -alpha, ply + 1, ctx);
                if (sc > alpha && sc < beta)
                    sc = -negamax(b, depth - 1 + mext, -beta, -alpha, ply + 1, ctx);
            }
        }
        b.unmake_move(m, u);
        move_idx++;

        if (ctx.stop) { ctx.rep.pop_back(); return (best == -INF) ? 0 : best; }
        if (sc > best) {
            best = sc; best_move = m;
            ctx.pv_table[ply][0] = m;
            int clen = ctx.pv_len[ply + 1];
            for (int i = 0; i < clen && i + 1 < 128; i++)
                ctx.pv_table[ply][i + 1] = ctx.pv_table[ply + 1][i];
            ctx.pv_len[ply] = clen + 1;
        }
        if (sc > alpha) alpha = sc;
        first = false;
        if (alpha >= beta) {
            if (move_flag(m) == FLAG_NORMAL && ply < 128) {
                if (ctx.killers[ply][0] != m) {
                    ctx.killers[ply][1] = ctx.killers[ply][0];
                    ctx.killers[ply][0] = m;
                }
                // History: refuerza movimientos no-captura que causan cutoff.
                Square f = move_from(m), t = move_to(m);
                if (f >= 0 && f < 64 && t >= 0 && t < 64) {
                    int& hh = ctx.history[b.side][f][t];
                    hh += (depth * depth);
                    if (hh > 1000000) hh = 1000000;
                }
            }
            break;
        }
    }
    ctx.rep.pop_back();

    uint8_t flag = 0;
    if (best <= alpha_orig) flag = 2;
    else if (best >= beta) flag = 1;
    if (!ctx.stop)
        tt_store(h, depth, score_to_tt(best, ply), (uint32_t)best_move, flag);
    return best;
}

// ---------------- Búsqueda en la raíz ----------------
static Score root_search(Board& b, int depth, Score alpha, Score beta, Context& ctx, Move& best_out) {
    ctx.nodes++;
    ctx.pv_len[0] = 0;
    MoveList moves; b.legal_moves(moves);
    if (moves.empty()) {
        if (b.in_check()) return -MATE;
        return 0;
    }
    uint64_t h = b.zkey;
    TTEntry* tte = tt_probe(h);
    Move ttm = tte ? (Move)tte->move : 0;
    order_moves(b, moves, ttm, ctx, 0);

    Score best = -INF;
    Move best_move = 0;
    Score alpha_orig = alpha;
    for (int i = 0; i < moves.count; i++) {
        Move m = moves[i];
        Undo u; b.make_move(m, u);
        Score sc = -negamax(b, depth - 1, -beta, -alpha, 1, ctx);
        b.unmake_move(m, u);
        if (ctx.stop) break;
        if (sc > best) {
            best = sc;
            best_move = m;
            ctx.pv_table[0][0] = m;
            int clen = ctx.pv_len[1];
            for (int j = 0; j < clen && j + 1 < MAX_PLY; j++)
                ctx.pv_table[0][j + 1] = ctx.pv_table[1][j];
            ctx.pv_len[0] = clen + 1;
        }
        if (sc > alpha) alpha = sc;
        if (alpha >= beta) break;
    }
    best_out = best_move;
    // guardar en TT (solo si la iteración terminó completa)
    if (!ctx.stop) {
        uint8_t flag = 0;
        if (best <= alpha_orig) flag = 2;
        else if (best >= beta) flag = 1;
        tt_store(h, depth, score_to_tt(best, 0), (uint32_t)best_move, flag);
    }
    return best;
}

SearchResult search(Board& b, const SearchLimits& lim) {
    tt_init();
    Context ctx{};
    // Reinicia la heuristica de historia para esta búsqueda.
    std::memset(ctx.history, 0, sizeof(ctx.history));
    ctx.start_ms = now_ms();
    ctx.time_ms = lim.time_ms;
    ctx.has_time_limit = (lim.time_ms > 0);
    SearchResult res;
    res.best = 0; res.score = 0; res.depth = 0; res.nodes = 0;

    Move bm = 0;
    Score prev_score = 0;
    for (int d = 1; d <= lim.max_depth; d++) {
        Move cur = 0;
        Score sc;
        // ---- Ventanas de Aspiración ----
        // A partir de cierta profundidad, buscamos con una ventana estrecha
        // centrada en la puntuación de la iteración anterior. Si falla alto o
        // bajo, ensanchamos progresivamente hasta cubrir todo el rango.
        if (d >= 4 && prev_score > -MATE + 1000 && prev_score < MATE - 1000) {
            int delta = 30;
            Score a = prev_score - delta;
            Score be = prev_score + delta;
            while (true) {
                sc = root_search(b, d, a, be, ctx, cur);
                if (ctx.stop) break;
                if (sc <= a) {            // fail-low: ensanchar hacia abajo
                    a = std::max(-INF, a - delta * 2);
                    delta *= 2;
                } else if (sc >= be) {    // fail-high: ensanchar hacia arriba
                    be = std::min(INF, be + delta * 2);
                    delta *= 2;
                } else {
                    break;                // dentro de la ventana: aceptado
                }
                if (delta > 2000) {       // seguridad: ventana completa
                    sc = root_search(b, d, -INF, INF, ctx, cur);
                    break;
                }
            }
        } else {
            sc = root_search(b, d, -INF, INF, ctx, cur);
        }
        if (ctx.stop) break;
        bm = cur;
        prev_score = sc;
        res.best = bm; res.score = sc; res.depth = d; res.nodes = ctx.nodes;
        if (lim.on_info) {
            std::vector<Move> pv;
            for (int i = 0; i < ctx.pv_len[0]; i++) pv.push_back(ctx.pv_table[0][i]);
            lim.on_info(d, sc, ctx.nodes, now_ms() - ctx.start_ms, pv);
        }
        if (sc > MATE - 1000 || sc < -MATE + 1000) break; // mate encontrado
    }
    if (res.best == 0) {
        // No se halló mejor jugada (p.ej. la búsqueda fue interrumpida antes de
        // completar la primera iteración de la raíz). Nunca devolver 0 si existe
        // al menos un movimiento legal: caer al primero para no emitir "0000".
        auto lm = b.legal_moves();
        if (!lm.empty()) {
            res.best = lm[0];
            res.score = 0;
            res.depth = 1;
            res.nodes = ctx.nodes;
        }
    }
    return res;
}

void signal_stop() { g_stop_flag.store(true, std::memory_order_relaxed); }
void clear_stop() { g_stop_flag.store(false, std::memory_order_relaxed); }

std::string self_play(Board b, int max_depth, int time_ms, std::vector<std::string>* moves) {
    std::vector<std::string> mlist;
    std::vector<uint64_t> hashes;    // historial para detectar triple repetición
    auto flush = [&]() { if (moves) *moves = mlist; };
    int ply = 0;
    while (true) {
        if (b.halfmove >= 100) { flush(); return "1/2-1/2 (50-move)"; }
        if (insufficient_material(b)) { flush(); return "1/2-1/2 (material insuficiente)"; }
        // repetición triple real sobre el historial de la partida
        uint64_t h = b.zkey;
        int reps = 1;
        for (uint64_t prev : hashes) if (prev == h) reps++;
        if (reps >= 3) { flush(); return "1/2-1/2 (repetición)"; }
        hashes.push_back(h);
        // detectar ahogado / mate
        auto lm = b.legal_moves();
        if (lm.empty()) {
            if (b.in_check()) { flush(); return (b.side == WHITE) ? "0-1 (mate)" : "1-0 (mate)"; }
            flush(); return "1/2-1/2 (ahogado)";
        }
        // Comunica el historial real a la búsqueda para detectar repeticiones.
        set_game_history(hashes);
        SearchLimits lim; lim.max_depth = max_depth; lim.time_ms = time_ms;
        SearchResult r = search(b, lim);
        if (r.best == 0) { flush(); return "1/2-1/2"; }
        std::string uci = square_to_string(move_from(r.best)) + square_to_string(move_to(r.best));
        if (move_promo(r.best)) uci += "nbrq"[move_promo(r.best) - 1];
        mlist.push_back(uci);
        Undo u; b.make_move(r.best, u);
        ply++;
        if (ply > 400) { flush(); return "1/2-1/2 (límite de plies)"; }
    }
    flush();
    return "?";
}

} // namespace hy3
