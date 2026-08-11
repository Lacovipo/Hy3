// board.cpp - Implementación de la representación del tablero
#include "board.h"
#include "movegen.h"
#include <cstring>
#include <sstream>
#include <cassert>
#include <random>
#include <algorithm>

namespace hy3 {

// ---- Zobrist (para hash de posición / repetición / transposición) ----
// 14 filas: la codificación de pieza es color*8+tipo, por lo que las piezas
// negras ocupan los índices 8..13 (los índices 6,7 quedan sin uso).
static uint64_t ZOB_PIECE[14][64];
static uint64_t ZOB_SIDE;
static uint64_t ZOB_EP[64];
static uint64_t ZOB_CASTLE[16];
static bool zob_ready = false;

static void init_zobrist() {
    if (zob_ready) return;
    std::mt19937_64 rng(0x1234567890ABCDEFULL);
    for (int p = 0; p < 14; p++)
        for (int s = 0; s < 64; s++)
            ZOB_PIECE[p][s] = rng();
    ZOB_SIDE = rng();
    for (int s = 0; s < 64; s++) ZOB_EP[s] = rng();
    for (int c = 0; c < 16; c++) ZOB_CASTLE[c] = rng();
    zob_ready = true;
}

uint64_t Board::hash() const {
    init_zobrist();
    uint64_t h = 0;
    for (int s = 0; s < 64; s++) {
        Piece p = squares[s];
        if (piece_type(p) != NONE)
            h ^= ZOB_PIECE[p][s];
    }
    if (ep_square != NO_SQ) h ^= ZOB_EP[ep_square];
    h ^= ZOB_CASTLE[castling & 15];
    if (side == BLACK) h ^= ZOB_SIDE;
    return h;
}

void Board::recompute_zkey() {
    zkey = hash();
}

int Board::legal_move_count() const {
    MoveList pseudo;
    generate_pseudo_moves(*this, pseudo);
    int n = 0;
    for (int i = 0; i < pseudo.count; i++) if (is_legal(pseudo[i])) n++;
    return n;
}

static long g_nmake = 0;
static long g_nunmake = 0;
void debug_counts(long& mk, long& um) { mk = g_nmake; um = g_nunmake; }

Board::Board() {
    set_from_fen("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");
}

Board::Board(const std::string& fen) {
    set_from_fen(fen);
}

void Board::set_from_fen(const std::string& fen) {
    for (int i = 0; i < N_SQ; i++) squares[i] = EMPTY;
    side = WHITE;
    ep_square = NO_SQ;
    castling = 0;
    halfmove = 0;
    fullmove = 1;
    king_sq[0] = king_sq[1] = NO_SQ;

    std::istringstream iss(fen);
    std::string board_part, side_part, castle_part, ep_part, half_part, full_part;
    iss >> board_part >> side_part >> castle_part >> ep_part >> half_part >> full_part;

    int rank = 7, file = 0;
    for (char c : board_part) {
        if (c == '/') { rank--; file = 0; if (rank < 0) break; }
        else if (c >= '1' && c <= '8') { file += (c - '0'); }
        else {
            // Guarda de rango: un FEN malformado con más de 8 piezas en una
            // fila (o más de 8 filas) escribiría fuera de squares[64].
            if (file < 0 || file > 7 || rank < 0 || rank > 7) { file++; continue; }
            Color col = (c >= 'a' && c <= 'z') ? BLACK : WHITE;
            PieceType pt;
            switch (tolower(c)) {
                case 'p': pt = PAWN; break;
                case 'n': pt = KNIGHT; break;
                case 'b': pt = BISHOP; break;
                case 'r': pt = ROOK; break;
                case 'q': pt = QUEEN; break;
                case 'k': pt = KING; break;
                default: pt = NONE; break;
            }
            if (pt == NONE) { file++; continue; }   // carácter no reconocido
            Piece p = make_piece(col, pt);
            Square sq = make_sq(file, rank);
            squares[sq] = p;
            if (pt == KING) king_sq[col] = sq;
            file++;
        }
    }

    side = (side_part == "b") ? BLACK : WHITE;
    for (char c : castle_part) {
        if (c == 'K') castling |= 1;
        else if (c == 'Q') castling |= 2;
        else if (c == 'k') castling |= 4;
        else if (c == 'q') castling |= 8;
    }
    // Casilla al paso: validar longitud y rango antes de convertir. Un FEN
    // truncado ("... w KQkq" sin campo ep) producía un índice basura que luego
    // se usaba para indexar ZOB_EP[64].
    ep_square = NO_SQ;
    if (ep_part.size() >= 2 && ep_part[0] >= 'a' && ep_part[0] <= 'h'
        && ep_part[1] >= '1' && ep_part[1] <= '8') {
        ep_square = string_to_square(ep_part);
    }
    try { if (!half_part.empty()) halfmove = std::stoi(half_part); } catch (...) { halfmove = 0; }
    try { if (!full_part.empty()) fullmove = std::stoi(full_part); } catch (...) { fullmove = 1; }
    if (halfmove < 0) halfmove = 0;
    if (fullmove < 1) fullmove = 1;

    // Validación mínima: debe haber exactamente un rey por bando; si no, el
    // FEN está corrupto y dejaríamos king_sq desfasado. En ese caso descartamos
    // el castling para no generar enroques imposibles.
    int wk = 0, bk = 0;
    for (int s = 0; s < N_SQ; s++) {
        Piece p = squares[s];
        if (piece_type(p) == KING) {
            if (piece_color(p) == WHITE) wk++;
            else bk++;
        }
    }
    if (wk != 1 || bk != 1) castling = 0;
    if (squares[make_sq(4,0)] != make_piece(WHITE, KING)) castling &= ~(1 | 2);
    if (squares[make_sq(7,0)] != make_piece(WHITE, ROOK)) castling &= ~1;
    if (squares[make_sq(0,0)] != make_piece(WHITE, ROOK)) castling &= ~2;
    if (squares[make_sq(4,7)] != make_piece(BLACK, KING)) castling &= ~(4 | 8);
    if (squares[make_sq(7,7)] != make_piece(BLACK, ROOK)) castling &= ~4;
    if (squares[make_sq(0,7)] != make_piece(BLACK, ROOK)) castling &= ~8;

    recompute_zkey();   // inicializa la clave Zobrist incremental
}

std::string Board::to_fen() const {
    std::string s;
    for (int r = 7; r >= 0; r--) {
        int empty = 0;
        for (int f = 0; f < 8; f++) {
            Piece p = squares[make_sq(f, r)];
            if (piece_type(p) == NONE) { empty++; }
            else {
                if (empty) { s += std::to_string(empty); empty = 0; }
                char c = "PNBRQK"[piece_type(p)];
                if (piece_color(p) == BLACK) c = tolower(c);
                s += c;
            }
        }
        if (empty) s += std::to_string(empty);
        if (r > 0) s += '/';
    }
    s += ' ';
    s += (side == WHITE) ? 'w' : 'b';
    s += ' ';
    if (castling == 0) s += '-';
    else {
        if (castling & 1) s += 'K';
        if (castling & 2) s += 'Q';
        if (castling & 4) s += 'k';
        if (castling & 8) s += 'q';
    }
    s += ' ';
    if (ep_square == NO_SQ) s += '-';
    else s += square_to_string(ep_square);
    s += ' ';
    s += std::to_string(halfmove);
    s += ' ';
    s += std::to_string(fullmove);
    return s;
}

void Board::make_move(Move m, Undo& u) {
    g_nmake++;
    init_zobrist();
    Square from = move_from(m);
    Square to = move_to(m);
    int flag = move_flag(m);
    int promo = move_promo(m);

    u.ep_square = ep_square;
    u.castling = castling;
    u.halfmove = halfmove;
    u.move_flag = flag;
    u.promo_piece = make_piece(side, NONE);
    u.zkey = zkey;                    // clave anterior (restauración O(1))

    uint64_t k = zkey;
    // XOR de una pieza sobre una casilla (ignora casillas vacías, igual que hash()).
    auto tog = [&](Piece p, Square s) { if (piece_type(p) != NONE) k ^= ZOB_PIECE[p][s]; };

    Piece moving = squares[from];
    PieceType pt = piece_type(moving);

    // Captura al paso: la pieza capturada está en (to_file, from_rank)
    if (flag == FLAG_EP) {
        Square cap_sq = make_sq(sq_file(to), sq_rank(from));
        u.captured = squares[cap_sq];
        tog(u.captured, cap_sq);
        squares[cap_sq] = EMPTY;
    } else {
        u.captured = squares[to];
        tog(u.captured, to);          // retira la pieza capturada (si la hay)
    }

    // Mover pieza (retirar de 'from')
    tog(moving, from);
    squares[to] = moving;
    squares[from] = EMPTY;

    // Promoción
    if (promo != 0) {
        PieceType npt = (PieceType)promo; // promo: 1=knight,2=bishop,3=rook,4=queen ( == PieceType )
        squares[to] = make_piece(side, npt);
        u.promo_piece = squares[to];
        tog(squares[to], to);         // añade la pieza promocionada en 'to'
    } else {
        tog(moving, to);              // añade la pieza que se movió en 'to'
    }

    // Enroque: mover torre
    if (flag == FLAG_CASTLE) {
        Square rf = NO_SQ, rt = NO_SQ;
        if (to == make_sq(6, 0))      { rf = make_sq(7,0); rt = make_sq(5,0); } // blancas corto
        else if (to == make_sq(2, 0)) { rf = make_sq(0,0); rt = make_sq(3,0); } // blancas largo
        else if (to == make_sq(6, 7)) { rf = make_sq(7,7); rt = make_sq(5,7); } // negras corto
        else if (to == make_sq(2, 7)) { rf = make_sq(0,7); rt = make_sq(3,7); } // negras largo
        if (rf != NO_SQ) {
            Piece rook = squares[rf];
            tog(rook, rf);
            squares[rt] = rook;
            squares[rf] = EMPTY;
            tog(rook, rt);
        }
    }

    // Actualizar posición del rey
    if (pt == KING) king_sq[side] = to;

    // Derechos de enroque (se recalculan; el hash se ajusta al final)
    if (pt == KING) {
        if (side == WHITE) castling &= ~(1 | 2);
        else castling &= ~(4 | 8);
    }
    // Se pierden si se mueve una torre desde su casilla original
    if (from == make_sq(0,0)) castling &= ~2;
    if (from == make_sq(7,0)) castling &= ~1;
    if (from == make_sq(0,7)) castling &= ~8;
    if (from == make_sq(7,7)) castling &= ~4;
    // Se pierden si se captura una torre en su casilla original
    if (to == make_sq(0,0)) castling &= ~2;
    if (to == make_sq(7,0)) castling &= ~1;
    if (to == make_sq(0,7)) castling &= ~8;
    if (to == make_sq(7,7)) castling &= ~4;

    // Ajuste Zobrist por cambio de derechos de enroque
    k ^= ZOB_CASTLE[u.castling & 15] ^ ZOB_CASTLE[castling & 15];

    // Ajuste Zobrist por ep antiguo (se retira siempre; se reañade si procede)
    if (u.ep_square != NO_SQ) k ^= ZOB_EP[u.ep_square];

    // Ep square: sólo se fija si un peón enemigo adyacente podría capturar al paso
    Color enemy = Color(side ^ 1);
    auto can_ep_capture = [&](int ff, int tr) {
        if (ff < 0 || ff > 7) return false;
        Piece p = squares[make_sq(ff, tr)];
        return piece_type(p) == PAWN && piece_color(p) == enemy;
    };
    if (pt == PAWN && sq_rank(to) - sq_rank(from) == 2) {
        int ff = sq_file(from), tr = sq_rank(to);
        ep_square = (can_ep_capture(ff - 1, tr) || can_ep_capture(ff + 1, tr))
            ? make_sq(ff, sq_rank(from) + 1) : NO_SQ;
    } else if (pt == PAWN && sq_rank(from) - sq_rank(to) == 2) {
        int ff = sq_file(from), tr = sq_rank(to);
        ep_square = (can_ep_capture(ff - 1, tr) || can_ep_capture(ff + 1, tr))
            ? make_sq(ff, sq_rank(from) - 1) : NO_SQ;
    } else {
        ep_square = NO_SQ;
    }
    if (ep_square != NO_SQ) k ^= ZOB_EP[ep_square];

    // Reloj de mediacaptura
    if (pt == PAWN || u.captured != EMPTY || flag == FLAG_EP)
        halfmove = 0;
    else
        halfmove++;

    if (side == BLACK) fullmove++;
    side = Color(side ^ 1);
    k ^= ZOB_SIDE;                    // el bando siempre cambia
    zkey = k;
}

void Board::unmake_move(Move m, const Undo& u) {
    g_nunmake++;
    Square from = move_from(m);
    Square to = move_to(m);
    int flag = u.move_flag;

    side = Color(side ^ 1);
    if (side == BLACK) fullmove--;

    Piece moving = squares[to];
    // Si fue promoción, la pieza en 'to' es la promocionada; restauramos peón
    if (piece_type(u.promo_piece) != NONE) {
        moving = make_piece(side, PAWN);
    }

    squares[from] = moving;
    squares[to] = EMPTY;

    // Restaurar capturada
    if (u.captured != EMPTY) {
        if (flag == FLAG_EP) {
            Square cap_sq = make_sq(sq_file(to), sq_rank(from));
            squares[cap_sq] = u.captured;
        } else {
            squares[to] = u.captured;
        }
    }

    // Deshacer enroque (torre vuelve)
    if (flag == FLAG_CASTLE) {
        if (to == make_sq(6, 0)) {
            squares[make_sq(7,0)] = squares[make_sq(5,0)];
            squares[make_sq(5,0)] = EMPTY;
        } else if (to == make_sq(2, 0)) {
            squares[make_sq(0,0)] = squares[make_sq(3,0)];
            squares[make_sq(3,0)] = EMPTY;
        } else if (to == make_sq(6, 7)) {
            squares[make_sq(7,7)] = squares[make_sq(5,7)];
            squares[make_sq(5,7)] = EMPTY;
        } else if (to == make_sq(2, 7)) {
            squares[make_sq(0,7)] = squares[make_sq(3,7)];
            squares[make_sq(3,7)] = EMPTY;
        }
    }

    if (piece_type(moving) == KING) king_sq[side] = from;

    ep_square = u.ep_square;
    castling = u.castling;
    halfmove = u.halfmove;
    zkey = u.zkey;                    // restauración O(1) del hash incremental
}

bool Board::is_square_attacked(Square sq, Color by) const {
    int f = sq_file(sq), r = sq_rank(sq);

    // Peones: un peón de color 'by' situado en (pf,pr) ataca sq si pr == r - dir
    // Peón blanco (rank crece hacia arriba) ataca un rank por encima: dir = +1
    int dir = (by == WHITE) ? 1 : -1;
    for (int df : {-1, 1}) {
        int pf = f + df, pr = r - dir;
        if (pf >= 0 && pf < 8 && pr >= 0 && pr < 8) {
            Piece p = squares[make_sq(pf, pr)];
            if (piece_color(p) == by && piece_type(p) == PAWN) return true;
        }
    }
    // Caballos
    static const int kn[8][2] = {{1,2},{2,1},{2,-1},{1,-2},{-1,-2},{-2,-1},{-2,1},{-1,2}};
    for (auto& d : kn) {
        int nf = f + d[0], nr = r + d[1];
        if (nf>=0&&nf<8&&nr>=0&&nr<8) {
            Piece p = squares[make_sq(nf,nr)];
            if (piece_color(p)==by && piece_type(p)==KNIGHT) return true;
        }
    }
    // Rey
    for (int df=-1; df<=1; df++) for (int dr=-1; dr<=1; dr++) {
        if (df==0&&dr==0) continue;
        int nf=f+df, nr=r+dr;
        if (nf>=0&&nf<8&&nr>=0&&nr<8) {
            Piece p = squares[make_sq(nf,nr)];
            if (piece_color(p)==by && piece_type(p)==KING) return true;
        }
    }
    // Diagonales (alfil/dama)
    static const int di[4][2] = {{1,1},{1,-1},{-1,1},{-1,-1}};
    for (auto& d : di) {
        int nf=f+d[0], nr=r+d[1];
        while (nf>=0&&nf<8&&nr>=0&&nr<8) {
            Piece p = squares[make_sq(nf,nr)];
            if (piece_type(p)!=NONE) {
                if (piece_color(p)==by && (piece_type(p)==BISHOP||piece_type(p)==QUEEN)) return true;
                break;
            }
            nf+=d[0]; nr+=d[1];
        }
    }
    // Rectas (torre/dama)
    static const int li[4][2] = {{1,0},{-1,0},{0,1},{0,-1}};
    for (auto& d : li) {
        int nf=f+d[0], nr=r+d[1];
        while (nf>=0&&nf<8&&nr>=0&&nr<8) {
            Piece p = squares[make_sq(nf,nr)];
            if (piece_type(p)!=NONE) {
                if (piece_color(p)==by && (piece_type(p)==ROOK||piece_type(p)==QUEEN)) return true;
                break;
            }
            nf+=d[0]; nr+=d[1];
        }
    }
    return false;
}

std::vector<Move> Board::pseudo_moves() const {
    return generate_pseudo_moves(*this);
}

std::vector<Move> Board::legal_moves() const {
    std::vector<Move> pseudo = generate_pseudo_moves(*this);
    std::vector<Move> legal;
    for (Move m : pseudo) {
        if (is_legal(m)) legal.push_back(m);
    }
    return legal;
}

void Board::legal_moves(MoveList& out) const {
    MoveList pseudo;
    generate_pseudo_moves(*this, pseudo);
    out.clear();
    for (int i = 0; i < pseudo.count; i++)
        if (is_legal(pseudo[i])) out.push_back(pseudo[i]);
}

bool Board::is_legal(Move m) const {
    // Hacer el movimiento in-place y deshacerlo (unmake_move restaura todo el
    // estado vía Undo), evitando clonar el tablero por cada pseudo-movimiento.
    Board& self = const_cast<Board&>(*this);
    Undo u;
    Color us = side;
    self.make_move(m, u);
    bool ok = !self.in_check(us);
    self.unmake_move(m, u);
    return ok;
}

std::string square_to_string(Square s) {
    char f = 'a' + sq_file(s);
    char r = '1' + sq_rank(s);
    return std::string(1, f) + r;
}

Square string_to_square(const std::string& s) {
    int f = s[0] - 'a';
    int r = s[1] - '1';
    return make_sq(f, r);
}

// ---------------------------------------------------------------------------
// Static Exchange Evaluation (SEE)
// ---------------------------------------------------------------------------
// Devuelve el balance neto (en centipeones) de la secuencia de capturas sobre
// una casilla, asumiendo que ambos bandos aportan siempre su atacante más
// barato. v1.6 corrige tres defectos de la versión 1.5:
//   1. see_move(): el intercambio arranca con la pieza que REALMENTE mueve, no
//      con el atacante más barato. Antes Dxd5 y exd5 puntuaban igual.
//   2. El rey no puede iniciar ni continuar una captura sobre una casilla que
//      el rival todavía defiende (sería un movimiento ilegal).
//   3. Ataques a rayos-X (x-ray): al retirar un atacante de una línea se
//      reevalúan las piezas deslizantes que quedan detrás.
namespace {
    const int SEE_VAL[6] = { 100, 320, 330, 500, 900, 20000 };
    inline int see_pv(Piece p) {
        int t = piece_type(p);
        return (t >= 0 && t < 6) ? SEE_VAL[t] : 0;
    }

    // Encuentra el atacante más barato de color 'by' sobre 'to' en el tablero
    // mutable 'occ'. Devuelve -1 si ninguno. Al recorrer las líneas rectas y
    // diagonales sobre 'occ' (que se va vaciando), los ataques a rayos-X se
    // descubren de forma natural en las iteraciones siguientes.
    Square find_least_attacker(Color by, Square to, const Piece occ[64]) {
        int best_sq = -1;
        int best_val = 1 << 30;
        int dir = (by == WHITE) ? 1 : -1;
        for (int df : {-1, 1}) {
            int f = sq_file(to) + df, r = sq_rank(to) - dir;
            if (f >= 0 && f < 8 && r >= 0 && r < 8) {
                Square s = make_sq(f, r);
                if (piece_type(occ[s]) == PAWN && piece_color(occ[s]) == by) {
                    if (SEE_VAL[PAWN] < best_val) { best_val = SEE_VAL[PAWN]; best_sq = s; }
                }
            }
        }
        static const int kn[8][2] = {{1,2},{2,1},{2,-1},{1,-2},{-1,-2},{-2,-1},{-2,1},{-1,2}};
        for (auto& d : kn) {
            int f = sq_file(to) + d[0], r = sq_rank(to) + d[1];
            if (f>=0&&f<8&&r>=0&&r<8) {
                Square s = make_sq(f, r);
                if (piece_type(occ[s]) == KNIGHT && piece_color(occ[s]) == by) {
                    if (SEE_VAL[KNIGHT] < best_val) { best_val = SEE_VAL[KNIGHT]; best_sq = s; }
                }
            }
        }
        static const int di[4][2] = {{1,1},{1,-1},{-1,1},{-1,-1}};
        for (auto& d : di) {
            int f = sq_file(to)+d[0], r = sq_rank(to)+d[1];
            while (f>=0&&f<8&&r>=0&&r<8) {
                Piece p = occ[make_sq(f,r)];
                if (piece_type(p) != NONE) {
                    if (piece_color(p)==by && (piece_type(p)==BISHOP||piece_type(p)==QUEEN)) {
                        int pv = SEE_VAL[piece_type(p)];
                        if (pv < best_val) { best_val = pv; best_sq = make_sq(f,r); }
                    }
                    break;
                }
                f+=d[0]; r+=d[1];
            }
        }
        static const int li[4][2] = {{1,0},{-1,0},{0,1},{0,-1}};
        for (auto& d : li) {
            int f = sq_file(to)+d[0], r = sq_rank(to)+d[1];
            while (f>=0&&f<8&&r>=0&&r<8) {
                Piece p = occ[make_sq(f,r)];
                if (piece_type(p) != NONE) {
                    if (piece_color(p)==by && (piece_type(p)==ROOK||piece_type(p)==QUEEN)) {
                        int pv = SEE_VAL[piece_type(p)];
                        if (pv < best_val) { best_val = pv; best_sq = make_sq(f,r); }
                    }
                    break;
                }
                f+=d[0]; r+=d[1];
            }
        }
        // El rey solo entra en el intercambio si no hay otro atacante más
        // barato; se filtra después contra la defensa rival.
        for (int df=-1; df<=1; df++) for (int dr=-1; dr<=1; dr++) {
            if (df==0&&dr==0) continue;
            int f = sq_file(to)+df, r = sq_rank(to)+dr;
            if (f>=0&&f<8&&r>=0&&r<8) {
                Square s = make_sq(f, r);
                if (piece_type(occ[s]) == KING && piece_color(occ[s]) == by) {
                    if (SEE_VAL[KING] < best_val) { best_val = SEE_VAL[KING]; best_sq = s; }
                }
            }
        }
        return best_sq;
    }

    // ¿Queda algún atacante de color 'by' sobre 'to'? (para vetar capturas del
    // rey sobre casillas todavía defendidas).
    inline bool any_attacker(Color by, Square to, const Piece occ[64]) {
        return find_least_attacker(by, to, occ) >= 0;
    }

    // Núcleo del intercambio. 'first_from' es la casilla del primer atacante
    // (ya decidido por el llamante) y 'first_victim_val' el valor de la pieza
    // capturada en el primer golpe.
    int see_exchange(Piece occ[64], Square to, Color us,
                     Square first_from, int first_victim_val) {
        int gain[64];
        int n = 0;
        gain[n++] = first_victim_val;

        // La pieza que ocupa 'to' tras el primer golpe es la que arriesgamos.
        int on_square = see_pv(occ[first_from]);
        occ[to] = occ[first_from];
        occ[first_from] = EMPTY;

        Color side = Color(us ^ 1);
        while (n < 63) {
            Square from = find_least_attacker(side, to, occ);
            if (from < 0) break;
            // El rey no puede capturar si el bando contrario aún defiende.
            if (piece_type(occ[from]) == KING) {
                Piece save = occ[to];
                occ[to] = EMPTY;
                bool defended = any_attacker(Color(side ^ 1), to, occ);
                occ[to] = save;
                if (defended) break;
            }
            gain[n] = on_square - gain[n - 1];
            on_square = see_pv(occ[from]);
            occ[to] = occ[from];
            occ[from] = EMPTY;
            n++;
            side = Color(side ^ 1);
        }

        // Resolución hacia atrás: cada bando puede detenerse si le conviene.
        for (int i = n - 2; i >= 0; i--)
            gain[i] = -std::max(-gain[i], gain[i + 1]);
        return gain[0];
    }
}

int Board::see(Square to, Color us) const {
    Piece occ[64];
    for (int i = 0; i < 64; i++) occ[i] = squares[i];

    Square from = find_least_attacker(us, to, occ);
    if (from < 0) return 0;                  // 'us' no puede capturar
    if (piece_type(occ[from]) == KING) {
        Piece save = occ[to];
        occ[to] = EMPTY;
        bool defended = any_attacker(Color(us ^ 1), to, occ);
        occ[to] = save;
        if (defended) return 0;              // el rey no puede capturar ahí
    }
    return see_exchange(occ, to, us, from, see_pv(occ[to]));
}

int Board::see_move(Move m) const {
    Square from = move_from(m);
    Square to   = move_to(m);
    int flag    = move_flag(m);
    int promo   = move_promo(m);

    Piece occ[64];
    for (int i = 0; i < 64; i++) occ[i] = squares[i];

    Color us = piece_color(squares[from]);
    int victim_val = 0;

    if (flag == FLAG_EP) {
        // La víctima está en (columna de 'to', fila de 'from'); 'to' está vacía.
        Square cap_sq = make_sq(sq_file(to), sq_rank(from));
        victim_val = see_pv(occ[cap_sq]);
        occ[cap_sq] = EMPTY;
    } else {
        victim_val = see_pv(occ[to]);
    }

    // Una promoción cambia la pieza que queda expuesta en 'to' y añade
    // la diferencia de material del ascenso.
    if (promo != 0) {
        int gain_promo = SEE_VAL[promo] - SEE_VAL[PAWN];
        victim_val += gain_promo;
        occ[from] = make_piece(us, (PieceType)promo);
    }

    if (piece_type(occ[from]) == KING) {
        Piece save = occ[to];
        occ[to] = EMPTY;
        bool defended = any_attacker(Color(us ^ 1), to, occ);
        occ[to] = save;
        if (defended) return -SEE_VAL[KING];  // captura ilegal del rey
    }

    return see_exchange(occ, to, us, from, victim_val);
}

// Accesores Zobrist para la actualización incremental del null-move.
uint64_t zobrist_side() { return ZOB_SIDE; }
uint64_t zobrist_ep(Square s) { return (s >= 0 && s < 64) ? ZOB_EP[s] : 0ULL; }

} // namespace hy3
