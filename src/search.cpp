// search.cpp - Implementación de la búsqueda
#include <cmath>
#include "search.h"
#include <iostream>
#include <chrono>
#include <algorithm>
#include <atomic>
#include <cstring>
#if defined(_MSC_VER) && defined(_WIN64)
#include <intrin.h>   // _umul128 para el índice de la TT (tt_idx)
#endif

namespace hy3 {

bool insufficient_material(const Board& b) {
    int n[2][6] = {0};
    int total = 0;
    int bishop_sq_color[2] = { -1, -1 };
    bool two_bishop_colors[2] = { false, false };
    for (int sq = 0; sq < 64; sq++) {
        Piece p = b.squares[sq];
        if (piece_type(p) == NONE) continue;
        int c = piece_color(p), t = piece_type(p);
        n[c][t]++;
        if (t == BISHOP) {
            int col = (sq_file(sq) + sq_rank(sq)) & 1;
            if (bishop_sq_color[c] < 0) bishop_sq_color[c] = col;
            else if (bishop_sq_color[c] != col) two_bishop_colors[c] = true;
        }
        total++;
    }
    // Con cualquier peón, torre o dama sobre el tablero hay material suficiente.
    if (n[0][PAWN] || n[1][PAWN] || n[0][ROOK] || n[1][ROOK] || n[0][QUEEN] || n[1][QUEEN])
        return false;

    if (total <= 2) return true;                       // K vs K
    if (total == 3)                                    // K+N vs K, K+B vs K
        return (n[0][KNIGHT] + n[0][BISHOP] + n[1][KNIGHT] + n[1][BISHOP] == 1);
    if (total == 4) {
        // K+B vs K+B con ambos alfiles en casillas del mismo color: tablas.
        if (n[0][BISHOP] == 1 && n[1][BISHOP] == 1 &&
            n[0][KNIGHT] == 0 && n[1][KNIGHT] == 0 &&
            bishop_sq_color[0] == bishop_sq_color[1]) return true;
        // K+N+N vs K: no se puede forzar mate.
        if ((n[0][KNIGHT] == 2 && n[0][BISHOP] == 0 && n[1][KNIGHT] + n[1][BISHOP] == 0) ||
            (n[1][KNIGHT] == 2 && n[1][BISHOP] == 0 && n[0][KNIGHT] + n[0][BISHOP] == 0))
            return true;
    }
    // Varios alfiles del mismo bando, todos en el mismo color de casilla, vs rey solo.
    for (int c = 0; c < 2; c++) {
        int other = c ^ 1;
        if (n[other][KNIGHT] + n[other][BISHOP] == 0 &&
            n[c][KNIGHT] == 0 && n[c][BISHOP] >= 1 && !two_bishop_colors[c])
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
    uint8_t  age   = 0;    // generación de búsqueda (envejecimiento)
};
struct TTBucket { TTEntry e[2]; };

static std::vector<TTBucket> tt;
static size_t TT_BUCKETS = 0;
static int    g_tt_mb = 64;      // tamaño por defecto (MB)
static uint8_t g_tt_age = 0;     // generación actual

static void tt_alloc(int mb) {
    if (mb < 1) mb = 1;
    if (mb > 4096) mb = 4096;
    g_tt_mb = mb;
    // v1.5 redondeaba a la potencia de 2 inferior y desperdiciaba hasta el 25 %
    // de la memoria pedida. v1.6 usa TODOS los buckets que caben y mapea el
    // hash con una multiplicación de 128 bits (evita el módulo caro).
    size_t bytes = (size_t)mb * 1024 * 1024;
    size_t nbuckets = bytes / sizeof(TTBucket);
    if (nbuckets < 1024) nbuckets = 1024;
    tt.assign(nbuckets, TTBucket{});
    TT_BUCKETS = nbuckets;
    g_tt_age = 0;
}

static void tt_init() {
    if (!tt.empty()) return;
    tt_alloc(g_tt_mb);
}

void tt_resize(int mb) { tt_alloc(mb); }

// Mapea la clave al rango [0, TT_BUCKETS) sin módulo: toma los 64 bits altos
// del producto de 128 bits h * TT_BUCKETS. Portable: MSVC no soporta __int128,
// así que en esa plataforma usa la intrínseca _umul128.
static inline size_t tt_idx(uint64_t h) {
#if defined(_MSC_VER) && defined(_WIN64)
    uint64_t hi;
    _umul128(h, (uint64_t)TT_BUCKETS, &hi);
    return (size_t)hi;
#else
    return (size_t)(((unsigned __int128)h * (unsigned __int128)TT_BUCKETS) >> 64);
#endif
}

// Avanza la generación de la TT (una por jugada real, no por iteración).
void tt_new_search() { g_tt_age++; }

void tt_clear() {
    if (tt.empty()) return;
    std::fill(tt.begin(), tt.end(), TTBucket{});
    g_tt_age = 0;
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
    // Misma posición ya presente: actualizar in situ, pero sin degradar una
    // entrada más profunda de la generación actual (v1.5 se pisaba a sí misma
    // con búsquedas menos profundas).
    for (int i = 0; i < 2; i++) {
        if (b.e[i].key == h) {
            if (depth >= b.e[i].depth || b.e[i].age != g_tt_age || flag == 0) {
                if (move == 0) move = b.e[i].move;   // conserva la jugada previa
                b.e[i].depth = (int16_t)depth; b.e[i].score = (int16_t)score;
                b.e[i].move = move; b.e[i].flag = flag; b.e[i].age = g_tt_age;
            }
            return;
        }
    }
    // Cubo 0: depth-preferred, con envejecimiento (una entrada de una jugada
    // anterior se considera reemplazable aunque sea más profunda).
    bool stale0 = (b.e[0].age != g_tt_age);
    if (b.e[0].key == 0 || stale0 || depth >= b.e[0].depth) {
        // La entrada desalojada baja al cubo 1 en vez de perderse.
        if (b.e[0].key != 0) b.e[1] = b.e[0];
        b.e[0].key = h; b.e[0].depth = (int16_t)depth;
        b.e[0].score = (int16_t)score; b.e[0].move = move;
        b.e[0].flag = flag; b.e[0].age = g_tt_age;
    } else {
        // Cubo 1: always-replace.
        b.e[1].key = h; b.e[1].depth = (int16_t)depth;
        b.e[1].score = (int16_t)score; b.e[1].move = move;
        b.e[1].flag = flag; b.e[1].age = g_tt_age;
    }
}

// Historial de claves Zobrist de la partida real (para triple repetición).
static std::vector<uint64_t> g_game_hist;
void set_game_history(const std::vector<uint64_t>& keys) { g_game_hist = keys; }

// Nº de veces que 'h' aparece en el historial real de la partida, limitando la
// búsqueda a las últimas 'halfmove' posiciones (más allá hubo un movimiento
// irreversible y no puede haber repetición).
static inline int game_hist_count(uint64_t h, int halfmove) {
    int n = 0;
    int lim = (int)g_game_hist.size();
    int start = lim - halfmove - 1;
    if (start < 0) start = 0;
    for (int i = lim - 1; i >= start; i--)
        if (g_game_hist[i] == h) n++;
    return n;
}

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
    int64_t max_time_ms = 0;    // techo duro (nunca sobrepasarlo)
    bool stop = false;
    std::vector<uint64_t> rep;                   // hashes en el camino actual
    Move killers[128][2];
    // History heuristic: historial[color][from][to] (piezas no-captura)
    int history[2][64][64];
    // Countermove: la refutación que funcionó frente a la jugada anterior.
    Move countermove[2][64][64];
    // Jugada que condujo a cada ply (para countermove y anti-ping-pong).
    Move move_at_ply[128];
    bool has_time_limit = true;
    int64_t max_nodes = 0;      // límite de nodos (0 = sin límite)
    // Tabla triangular de la Variante Principal (PV)
    Move pv_table[128][128];
    int pv_len[128];
    int root_halfmove = 0;      // reloj de 50 en la raíz (para acotar el historial)
};

static int64_t now_ms() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
}

static std::atomic<bool> g_stop_flag{false};
static std::atomic<bool> g_pondering{false};
static std::atomic<int64_t> g_ponder_start{0};
// Tiempo de ponder "gratis" (del rival) que no debe descontarse del reloj propio.
// Se acumula mientras se pondera y se resta del tiempo transcurrido para que el
// reloj propio arranque de verdad en el 'ponderhit'. (Fix PARA_EL_AUTOR #1 / #4)
static std::atomic<int64_t> g_ponder_offset{0};

static void end_ponder() {
    if (g_pondering.load(std::memory_order_relaxed)) {
        g_ponder_offset.fetch_add(now_ms() - g_ponder_start.load(std::memory_order_relaxed),
                                  std::memory_order_relaxed);
        g_pondering.store(false, std::memory_order_relaxed);
    }
}

void set_pondering(bool on) {
    if (on) {
        g_pondering.store(true, std::memory_order_relaxed);
        g_ponder_start.store(now_ms(), std::memory_order_relaxed);
        g_ponder_offset.store(0, std::memory_order_relaxed);
    } else {
        end_ponder();
    }
}

// El GUI confirmó la jugada ponderada: el tiempo de ponder fue gratis; el reloj
// propio empieza a correr ahora mismo. (Fix PARA_EL_AUTOR #1)
void ponder_hit() { end_ponder(); }

// ---- Traza del árbol de búsqueda (depuración) ----
static bool g_tree_debug = false;
static int  g_tree_max_depth = 3;
void set_tree_debug(bool on, int max_depth_print) {
    g_tree_debug = on;
    g_tree_max_depth = max_depth_print;
}

// Tiempo efectivamente consumido del reloj propio: descuenta el ponder gratis.
static int64_t effective_elapsed(Context& ctx) {
    return now_ms() - ctx.start_ms - g_ponder_offset.load(std::memory_order_relaxed);
}

static bool time_up(Context& ctx) {
    if (ctx.stop) return true;
    if (g_stop_flag.load(std::memory_order_relaxed)) { ctx.stop = true; return true; }
    // Límite de nodos (parada por 'go nodes N').
    if (ctx.max_nodes > 0 && ctx.nodes >= ctx.max_nodes) { ctx.stop = true; return true; }
    // En ponder el reloj propio no corre: seguimos pensando hasta stop/ponderhit.
    if (g_pondering.load(std::memory_order_relaxed)) return false;
    if (!ctx.has_time_limit) return false;
    int64_t elapsed = effective_elapsed(ctx);
    // Tope DURO: aborta la iteración en curso si se supera el techo y devuelve
    // la mejor jugada hallada hasta ahora. Evita sobrepasar el tiempo asignado
    // (Fix PARA_EL_AUTOR #2 / #4).
    if (elapsed >= ctx.max_time_ms) { ctx.stop = true; return true; }
    // Tope blando: señal para no iniciar otra iteración en el bucle de ID.
    if (elapsed >= ctx.time_ms) { ctx.stop = true; return true; }
    return false;
}

// ---------------- Ordenación de movimientos ----------------
// Escala v1.6 (bandas separadas y sin solapamientos):
//   TT                       2.000.000
//   promoción a dama         1.900.000
//   captura con SEE >= 0     1.500.000 + MVV-LVA
//   killer 1 / killer 2      1.400.000 / 1.390.000
//   countermove              1.380.000
//   quiets (history)             0 .. 1.000.000
//   captura con SEE < 0        -100.000 + MVV-LVA
static int move_order_score(const Board& b, Move m, Move tt_move, const Context& ctx, int ply) {
    if (tt_move != 0 && m == tt_move) return 2000000;
    int flag = move_flag(m);
    int promo = move_promo(m);

    if (flag == FLAG_CAPTURE || flag == FLAG_EP) {
        Square to = move_to(m);
        Piece victim = b.squares[to];
        // En una captura al paso la víctima no está en 'to' sino al lado.
        int vv = (flag == FLAG_EP) ? PIECE_VALUE[PAWN]
               : (piece_type(victim) == NONE ? 0 : PIECE_VALUE[piece_type(victim)]);
        int av = PIECE_VALUE[piece_type(b.squares[move_from(m)])];
        int mvvlva = vv * 16 - av / 10;
        // SEE de la JUGADA concreta (v1.5 usaba see(casilla) y daba la misma
        // puntuación a Dxd5 que a exd5).
        int see_score = b.see_move(m);
        if (see_score >= 0) return 1500000 + mvvlva;
        return -100000 + mvvlva;              // captura perdedora: al final
    }
    // Promoción sin captura: la dama es la única realmente prioritaria.
    if (promo != 0) {
        if (promo == QUEEN) return 1900000;
        return 1350000 + PIECE_VALUE[promo];
    }
    // killers
    if (ply < 128) {
        if (ctx.killers[ply][0] == m) return 1400000;
        if (ctx.killers[ply][1] == m) return 1390000;
    }
    // Countermove: refutación que ya funcionó contra la jugada anterior.
    if (ply > 0 && ply < 128) {
        Move prev = ctx.move_at_ply[ply - 1];
        if (prev != 0 && ctx.countermove[b.side][move_from(prev)][move_to(prev)] == m)
            return 1380000;
    }
    // history heuristic (movimientos no-captura)
    Square f = move_from(m), t = move_to(m);
    int score = (f >= 0 && f < 64 && t >= 0 && t < 64) ? ctx.history[b.side][f][t] : 0;
    if (score > 1000000) score = 1000000;
    // Anti-repetir: penaliza deshacer la jugada que nos trajo aquí (ping-pong).
    // v1.6 usa la jugada del ply anterior REAL, no una variable global que se
    // arrastraba entre subárboles hermanos.
    if (ply > 0 && ply < 128 && ctx.move_at_ply[ply - 1] != 0) {
        Move prev = ctx.move_at_ply[ply - 1];
        Square pf = move_from(prev), pt = move_to(prev);
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

// Notación UCI de una jugada (para la traza del árbol). Duplicado local para
// no crear una dependencia de search.cpp sobre uci.cpp.
static std::string dbg_move_str(Move m) {
    if (m == 0) return "0000";
    std::string s = square_to_string(move_from(m)) + square_to_string(move_to(m));
    int pr = move_promo(m);
    if (pr) { const char* c = " nbrq"; s += c[pr]; }
    return s;
}

// Tabla de reducciones LMR precalculada: LMR_TABLE[depth][move_index].
// Fórmula estándar: 0.75 + ln(d) * ln(m) / 2.25
static int LMR_TABLE[64][64];
static bool g_lmr_init = false;
static void init_lmr_table() {
    if (g_lmr_init) return;
    for (int d = 1; d < 64; d++)
        for (int m = 1; m < 64; m++)
            LMR_TABLE[d][m] = (int)(0.75 + std::log((double)d) * std::log((double)m) / 2.25);
    g_lmr_init = true;
}

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
            ctx.move_at_ply[ply] = m;
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

    // ---- Sonda de la TT en quiescence ----
    // Las entradas de QS se guardan con depth 0; solo se usan para cortar,
    // nunca para sustituir una entrada de búsqueda normal más profunda.
    Move ttm = 0;
    TTEntry* tte = tt_probe(h);
    if (tte) {
        ttm = (Move)tte->move;
        int sc = score_from_tt(tte->score, ply);
        if (tte->flag == 0) return sc;
        if (tte->flag == 1 && sc >= beta) return sc;
        if (tte->flag == 2 && sc <= alpha) return sc;
    }

    Score stand = evaluate(b);
    if (stand >= beta) return stand;
    if (stand > alpha) alpha = stand;

    // v1.6: generador dedicado de capturas/promociones. v1.5 generaba todas las
    // legales (~85 % descartadas) y además, si no había NINGUNA legal, no lo
    // detectaba aquí: el ahogado se puntuaba con la evaluación material.
    MoveList qm; generate_captures(b, qm);
    order_moves(b, qm, ttm, ctx, ply);

    Score alpha_orig = alpha;
    Score best = stand;
    Move best_move = 0;
    ctx.rep.push_back(h);
    for (int i = 0; i < qm.count; i++) {
        Move m = qm[i];
        if (!b.is_legal(m)) continue;              // el generador es pseudolegal

        // ---- Poda SEE: descarta capturas claramente perdedoras ----
        if (move_promo(m) == 0 && b.see_move(m) < 0) continue;

        // ---- Delta pruning: ni capturando la pieza se alcanza alpha ----
        {
            int victim = (move_flag(m) == FLAG_EP)
                       ? PIECE_VALUE[PAWN]
                       : (piece_type(b.squares[move_to(m)]) == NONE
                          ? 0 : PIECE_VALUE[piece_type(b.squares[move_to(m)])]);
            int promo_gain = move_promo(m) ? PIECE_VALUE[move_promo(m)] - PIECE_VALUE[PAWN] : 0;
            if (stand + victim + promo_gain + 200 < alpha) continue;
        }

        Undo u; b.make_move(m, u);
        ctx.move_at_ply[ply] = m;
        Score sc = -quiescence(b, -beta, -alpha, ply + 1, ctx);
        b.unmake_move(m, u);
        if (ctx.stop) { ctx.rep.pop_back(); return alpha; }
        if (sc > best) { best = sc; best_move = m; }
        if (sc > alpha) alpha = sc;
        if (alpha >= beta) break;
    }
    ctx.rep.pop_back();
    if (!ctx.stop) {
        uint8_t flag = (best >= beta) ? 1 : (best > alpha_orig ? 0 : 2);
        tt_store(h, 0, score_to_tt(best, ply), best_move, flag);
    }
    return best;
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
    // Repetición contra el historial REAL de la partida. v1.5 devolvía tablas
    // con UNA sola aparición previa, lo que hacía al motor rehuir posiciones
    // ganadoras ya visitadas una vez. v1.6 exige dos apariciones históricas
    // (que con la posición actual completan la triple repetición); el recorrido
    // se acota además por el reloj de 50 jugadas.
    if (ply > 0 && !g_game_hist.empty()) {
        if (game_hist_count(h, b.halfmove) >= 2) return 0;
    }
    // Regla de 50 jugadas. Ojo: si el bando en turno está en jaque y no tiene
    // salidas, es MATE y el mate prevalece sobre las tablas (v1.5 devolvía 0).
    if (b.halfmove >= 100) {
        if (!b.in_check() || b.legal_move_count() > 0) return 0;
    }
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
        // Guarda anti-ahogado: si el bando en turno no tiene jugadas legales,
        // la posición es tablas por ahogado (0), no la evaluación material.
        // v1.5 podaba estas posiciones devolviendo una ventaja inexistente.
        if (static_eval - margin >= beta) {
            if (b.legal_move_count() > 0) return static_eval - margin;
            return 0;
        }
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
    // v1.6: se elimina la comprobación de "rival en jaque" (imposible en una
    // posición legal con el turno correcto: sería un rey capturable) y se
    // añade la condición de que la evaluación estática supere beta.
    if (!is_pv && !in_check && depth >= 3 && static_eval >= beta &&
        beta > -MATE + 1000 && beta < MATE - 1000) {
        int nonPawn = 0;
        for (int sq = 0; sq < 64; sq++) {
            Piece p = b.squares[sq];
            if (piece_type(p) == NONE || piece_color(p) != b.side) continue;
            int t = piece_type(p);
            if (t == PAWN || t == KING) continue;
            nonPawn++;
        }
        if (nonPawn >= 2) {
            // Reducción adaptativa: más profunda cuanto mayor sea el margen.
            int R = 3 + depth / 6 + std::min((static_eval - beta) / 200, 3);
            if (R > depth - 1) R = depth - 1;
            // Reducimos la ventana (null-window) alrededor de beta.
            ctx.rep.push_back(h);
            Square saved_ep = b.ep_square;
            uint64_t saved_key = b.zkey;
            // Actualización INCREMENTAL de la clave: v1.5 llamaba a b.hash(),
            // que recorre las 64 casillas en cada nodo con null-move.
            b.zkey ^= zobrist_side();
            if (saved_ep != NO_SQ) b.zkey ^= zobrist_ep(saved_ep);
            b.ep_square = NO_SQ;
            b.side = Color(b.side ^ 1);  // null move (cambio de lado)
            ctx.move_at_ply[ply] = 0;    // el null-move no es una jugada real
            Score null_sc = -negamax(b, depth - 1 - R, -beta, -beta + 1, ply + 1, ctx);
            b.side = Color(b.side ^ 1);  // restaurar lado
            b.ep_square = saved_ep;
            b.zkey = saved_key;
            ctx.rep.pop_back();
            if (ctx.stop) return (best == -INF) ? 0 : best;
            if (null_sc >= beta) {
                // No devolver puntuaciones de mate procedentes de un null-move:
                // el mate podría no existir sin la jugada nula.
                if (null_sc >= MATE - 1000) null_sc = beta;
                // Verificación a profundidad reducida en el zugzwang probable
                // (finales con poco material), donde el null-move miente.
                if (depth >= 8 && nonPawn <= 3) {
                    Score v = negamax(b, depth - R - 1, beta - 1, beta, ply, ctx);
                    if (ctx.stop) return (best == -INF) ? 0 : best;
                    if (v < beta) goto nmp_failed;
                }
                if (!ctx.stop)
                    tt_store(h, depth, score_to_tt(null_sc, ply), 0, 1);
                return null_sc;
            }
        }
    }
nmp_failed:;

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
        ctx.move_at_ply[ply] = m;
        // Extensión por jaque: solo extendemos el movimiento que DA jaque,
        // nunca la evasión.
        bool gives_check = b.in_check();
        int mext = gives_check ? 1 : 0;
        Score sc;
        if (first) {
            sc = -negamax(b, depth - 1 + mext, -beta, -alpha, ply + 1, ctx);
        } else {
            // ---- Late Move Reductions (v1.6) ----
            // Reducción logarítmica: crece con la profundidad y el orden de la
            // jugada. v1.5 usaba 1-2 pliegues fijos y solo con FLAG_NORMAL, lo
            // que excluía las promociones tranquilas (que NO deben reducirse) y
            // reducía poco en profundidades altas.
            int reduction = 0;
            if (depth >= 3 && move_idx >= 3 && is_quiet && !in_check && !gives_check) {
                reduction = LMR_TABLE[std::min(depth, 63)][std::min(move_idx, 63)];
                // Menos reducción en nodos PV y para jugadas con buen historial.
                if (is_pv) reduction--;
                Square ff = move_from(m), tt2 = move_to(m);
                int hist = ctx.history[b.side ^ 1][ff][tt2];
                if (hist > 8000) reduction--;
                else if (hist < -8000) reduction++;
                if (ctx.killers[ply][0] == m || ctx.killers[ply][1] == m) reduction--;
                if (reduction < 0) reduction = 0;
                if (reduction > depth - 2) reduction = depth - 2;
            }

            // Búsqueda de ventana nula, reducida si procede.
            sc = -negamax(b, depth - 1 - reduction + mext, -alpha - 1, -alpha, ply + 1, ctx);

            // BUG CRÍTICO v1.5: la re-búsqueda se condicionaba a (sc < beta),
            // imposible en un nodo no-PV donde beta == alpha+1, de modo que la
            // reducción NUNCA se verificaba. v1.6 re-busca a profundidad
            // completa en cuanto la jugada reducida supera alpha.
            if (reduction > 0 && sc > alpha) {
                sc = -negamax(b, depth - 1 + mext, -alpha - 1, -alpha, ply + 1, ctx);
            }
            // Y si además abre la ventana en un nodo PV, se re-busca completa.
            if (sc > alpha && sc < beta) {
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
            if (is_quiet && ply < 128) {
                if (ctx.killers[ply][0] != m) {
                    ctx.killers[ply][1] = ctx.killers[ply][0];
                    ctx.killers[ply][0] = m;
                }
                // Countermove: registra esta jugada como refutación de la
                // jugada anterior del rival.
                if (ply > 0 && ctx.move_at_ply[ply - 1] != 0) {
                    Move prev = ctx.move_at_ply[ply - 1];
                    ctx.countermove[b.side][move_from(prev)][move_to(prev)] = m;
                }
                // History con DOBLE SIGNO (v1.6): premia la jugada que corta y
                // penaliza las jugadas silenciosas ya probadas que fallaron.
                // v1.5 solo premiaba, con lo que la tabla saturaba y perdía
                // capacidad de discriminación.
                int bonus = depth * depth;
                if (bonus > 1200) bonus = 1200;
                Square f = move_from(m), t = move_to(m);
                if (f >= 0 && f < 64 && t >= 0 && t < 64) {
                    int& hh = ctx.history[b.side][f][t];
                    hh += bonus - hh * bonus / 16384;      // actualización con decaimiento
                }
                for (int j = 0; j < mi; j++) {
                    Move q = moves[j];
                    if (move_flag(q) == FLAG_CAPTURE || move_flag(q) == FLAG_EP) continue;
                    if (move_promo(q) != 0) continue;
                    Square qf = move_from(q), qt = move_to(q);
                    if (qf < 0 || qf > 63 || qt < 0 || qt > 63) continue;
                    int& qh = ctx.history[b.side][qf][qt];
                    qh += -bonus - qh * bonus / 16384;
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
        ctx.move_at_ply[0] = m;
        // ---- PVS en la raíz (v1.6) ----
        // v1.5 buscaba TODAS las jugadas de la raíz con ventana completa. Con
        // PVS solo la primera usa ventana completa; el resto se refuta con
        // ventana nula y solo se re-buscan si la superan.
        Score sc;
        if (i == 0) {
            sc = -negamax(b, depth - 1, -beta, -alpha, 1, ctx);
        } else {
            sc = -negamax(b, depth - 1, -alpha - 1, -alpha, 1, ctx);
            if (sc > alpha && sc < beta)
                sc = -negamax(b, depth - 1, -beta, -alpha, 1, ctx);
        }
        b.unmake_move(m, u);
        // ---- Traza del árbol (depuración) ----
        // Petición explícita de la revisión humana: poder inspeccionar qué
        // valora el motor en cada jugada de la raíz.
        if (g_tree_debug && !ctx.stop) {
            std::cout << "info string tree depth " << depth
                      << " move " << dbg_move_str(m)
                      << " score " << sc
                      << " nodes " << ctx.nodes << std::endl;
        }
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
    init_lmr_table();
    tt_new_search();                 // envejece la TT: nueva jugada real
    Context ctx{};
    // Reinicia la heuristica de historia para esta búsqueda.
    std::memset(ctx.history, 0, sizeof(ctx.history));
    std::memset(ctx.countermove, 0, sizeof(ctx.countermove));
    std::memset(ctx.move_at_ply, 0, sizeof(ctx.move_at_ply));
    ctx.start_ms = now_ms();
    ctx.time_ms = lim.time_ms;
    ctx.max_time_ms = lim.max_time_ms > 0 ? lim.max_time_ms : lim.time_ms;
    ctx.has_time_limit = (lim.time_ms > 0);
    ctx.max_nodes = lim.max_nodes;
    ctx.root_halfmove = b.halfmove;
    // Reset del estado de ponder: el offset de ponder "gratis" sólo es válido
    // durante una búsqueda de ponder. Si venimos de una búsqueda previa (p.ej.
    // tras un juego con ponder) un offset obsoleto haría que time_up() viera un
    // tiempo transcurrido negativo y la búsqueda IGNORARA el límite de tiempo
    // -> pérdida por tiempo. (Fix PARA_EL_AUTOR #2 / #4)
    g_ponder_offset.store(0, std::memory_order_relaxed);
    if (!lim.ponder) g_pondering.store(false, std::memory_order_relaxed);
    if (lim.ponder) set_pondering(true);
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
        // Jugada de ponder: segundo movimiento de la PV.
        res.ponder = (ctx.pv_len[0] >= 2) ? ctx.pv_table[0][1] : 0;
        if (lim.on_info) {
            std::vector<Move> pv;
            for (int i = 0; i < ctx.pv_len[0]; i++) pv.push_back(ctx.pv_table[0][i]);
            lim.on_info(d, sc, ctx.nodes, now_ms() - ctx.start_ms, pv);
        }
        // ---- Parada por mate (v1.6) ----
        // v1.5 detenía la ID al hallar CUALQUIER mate, congelando p. ej. un
        // mate en 5 sin buscar el mate en 2 que aparecería más adelante.
        // v1.6 solo se detiene cuando la profundidad ya basta para demostrar
        // el mate encontrado (no hay margen para acortarlo).
        if (sc > MATE - 1000) {
            int mate_in = (MATE - sc + 1) / 2;
            if (d >= mate_in * 2) break;       // mate ya demostrado; no acortable
        } else if (sc < -MATE + 1000) {
            break;                             // estamos perdidos: nada que ganar
        }
        // ---- Gestión del tiempo (v1.6) ----
        // No iniciar una iteración que casi con seguridad no terminará: cada
        // profundidad cuesta ~2x la anterior. v1.5 arrancaba iteraciones que
        // luego abortaba, sobrepasando el presupuesto asignado.
        if (ctx.has_time_limit && !g_pondering.load(std::memory_order_relaxed)) {
            int64_t elapsed = effective_elapsed(ctx);
            if (elapsed * 2 >= ctx.time_ms) break;
        }
    }
    if (lim.ponder) set_pondering(false);
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
