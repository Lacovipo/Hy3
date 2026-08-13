// eval.cpp - Implementación de la evaluación (v1.6)
#include "eval.h"
#include "movegen.h"

namespace hy3 {

const int PIECE_VALUE[6] = { 100, 320, 330, 500, 900, 20000 };

// Valores de material separados por fase (v1.6). En el final el peón vale
// más (se acerca a coronar) y las piezas menores algo menos.
static const int MAT_MG[6] = { 82, 337, 365, 477, 1025, 20000 };
static const int MAT_EG[6] = { 94, 281, 297, 512,  936, 20000 };

// ---------------------------------------------------------------------------
// Piece-square tables separadas MG/EG (perspectiva blanca, a1=0 ... h8=63)
// ---------------------------------------------------------------------------
static const int PAWN_MG[64] = {
      0,   0,   0,   0,   0,   0,   0,   0,
    -35,  -1, -20, -23, -15,  24,  38, -22,
    -26,  -4,  -4, -10,   3,   3,  33, -12,
    -27,  -2,  -5,  12,  17,   6,  10, -25,
    -14,  13,   6,  21,  23,  12,  17, -23,
     -6,   7,  26,  31,  65,  56,  25, -20,
     98, 134,  61,  95,  68, 126,  34, -11,
      0,   0,   0,   0,   0,   0,   0,   0
};
static const int PAWN_EG[64] = {
      0,   0,   0,   0,   0,   0,   0,   0,
     13,   8,   8,  10,  13,   0,   2,  -7,
      4,   7,  -6,   1,   0,  -5,  -1,  -8,
     13,   9,  -3,  -7,  -7,  -8,   3,  -1,
     32,  24,  13,   5,  -2,   4,  17,  17,
     94, 100,  85,  67,  56,  53,  82,  84,
    178, 173, 158, 134, 147, 132, 165, 187,
      0,   0,   0,   0,   0,   0,   0,   0
};
static const int KNIGHT_MG[64] = {
   -105, -21, -58, -33, -17, -28, -19, -23,
    -29, -53, -12,  -3,  -1,  18, -14, -19,
    -23,  -9,  12,  10,  19,  17,  25, -16,
    -13,   4,  16,  13,  28,  19,  21,  -8,
     -9,  17,  19,  53,  37,  69,  18,  22,
    -47,  60,  37,  65,  84, 129,  73,  44,
    -73, -41,  72,  36,  23,  62,   7, -17,
   -167, -89, -34, -49,  61, -97, -15,-107
};
static const int KNIGHT_EG[64] = {
    -29, -51, -23, -15, -22, -18, -50, -64,
    -42, -20, -10,  -5,  -2, -20, -23, -44,
    -23,  -3,  -1,  15,  10,  -3, -20, -22,
    -18,  -6,  16,  25,  16,  17,   4, -18,
    -17,   3,  22,  22,  22,  11,   8, -18,
    -24, -20,  10,   9,  -1,  -9, -19, -41,
    -25,  -8, -25,  -2,  -9, -25, -24, -52,
    -58, -38, -13, -28, -31, -27, -63, -99
};
static const int BISHOP_MG[64] = {
    -33,  -3, -14, -21, -13, -12, -39, -21,
      4,  15,  16,   0,   7,  21,  33,   1,
      0,  15,  15,  15,  14,  27,  18,  10,
     -6,  13,  13,  26,  34,  12,  10,   4,
     -4,   5,  19,  50,  37,  37,   7,  -2,
    -16,  37,  43,  40,  35,  50,  37,  -2,
    -26,  16, -18, -13,  30,  59,  18, -47,
    -29,   4, -82, -37, -25, -42,   7,  -8
};
static const int BISHOP_EG[64] = {
    -23,  -9, -23,  -5,  -9, -16,  -5, -17,
    -14, -18,  -7,  -1,   4,  -9, -15, -27,
    -12,  -3,   8,  10,  13,   3,  -7, -15,
     -6,   3,  13,  19,   7,  10,  -3,  -9,
     -3,   9,  12,   9,  14,  10,   3,   2,
      2,  -8,   0,  -1,  -2,   6,   0,   4,
     -8,  -4,   7, -12,  -3, -13,  -4, -14,
    -14, -21, -11,  -8,  -7,  -9, -17, -24
};
static const int ROOK_MG[64] = {
    -19, -13,   1,  17,  16,   7, -37, -26,
    -44, -16, -20,  -9,  -1,  11,  -6, -71,
    -45, -25, -16, -17,   3,   0,  -5, -33,
    -36, -26, -12,  -1,   9,  -7,   6, -23,
    -24, -11,   7,  26,  24,  35,  -8, -20,
     -5,  19,  26,  36,  17,  45,  61,  16,
     27,  32,  58,  62,  80,  67,  26,  44,
     32,  42,  32,  51,  63,   9,  31,  43
};
static const int ROOK_EG[64] = {
     -9,   2,   3,  -1,  -5, -13,   4, -20,
     -6,  -6,   0,   2,  -9,  -9, -11,  -3,
     -4,   0,  -5,  -1,  -7, -12,  -8, -16,
      3,   5,   8,   4,  -5,  -6,  -8, -11,
      4,   3,  13,   1,   2,   1,  -1,   2,
      7,   7,   7,   5,   4,  -3,  -5,  -3,
     11,  13,  13,  11,  -3,   3,   8,   3,
     13,  10,  18,  15,  12,  12,   8,   5
};
static const int QUEEN_MG[64] = {
     -1, -18,  -9,  10, -15, -25, -31, -50,
    -35,  -8,  11,   2,   8,  15,  -3,   1,
    -14,   2, -11,  -2,  -5,   2,  14,   5,
     -9, -26,  -9, -10,  -2,  -4,   3,  -3,
    -27, -27, -16, -16,  -1,  17,  -2,   1,
    -13, -17,   7,   8,  29,  56,  47,  57,
    -24, -39,  -5,   1, -16,  57,  28,  54,
    -28,   0,  29,  12,  59,  44,  43,  45
};
static const int QUEEN_EG[64] = {
    -33, -28, -22, -43,  -5, -32, -20, -41,
    -22, -23, -30, -16, -16, -23, -36, -32,
    -16, -27,  15,   6,   9,  17,  10,   5,
    -18,  28,  19,  47,  31,  34,  39,  23,
      3,  22,  24,  45,  57,  40,  57,  36,
    -20,   6,   9,  49,  47,  35,  19,   9,
    -17,  20,  32,  41,  58,  25,  30,   0,
     -9,  22,  22,  27,  27,  19,  10,  20
};
static const int KING_MG[64] = {
    -15,  36,  12, -54,   8, -28,  24,  14,
      1,   7,  -8, -64, -43, -16,   9,   8,
    -14, -14, -22, -46, -44, -30, -15, -27,
    -49,  -1, -27, -39, -46, -44, -33, -51,
    -17, -20, -12, -27, -30, -25, -14, -36,
     -9,  24,   2, -16, -20,   6,  22, -22,
     29,  -1, -20,  -7,  -8,  -4, -38, -29,
    -65,  23,  16, -15, -56, -34,   2,  13
};
static const int KING_EG[64] = {
    -53, -34, -21, -11, -28, -14, -24, -43,
    -27, -11,   4,  13,  14,   4,  -5, -17,
    -19,  -3,  11,  21,  23,  16,   7,  -9,
    -18,  -4,  21,  24,  27,  23,   9, -11,
     -8,  22,  24,  27,  26,  33,  26,   3,
     10,  17,  23,  15,  20,  45,  44,  13,
    -12,  17,  14,  17,  17,  38,  23,  11,
    -74, -35, -18, -18, -11,  15,   4, -17
};

static const int* PST_MG[6] = { PAWN_MG, KNIGHT_MG, BISHOP_MG, ROOK_MG, QUEEN_MG, KING_MG };
static const int* PST_EG[6] = { PAWN_EG, KNIGHT_EG, BISHOP_EG, ROOK_EG, QUEEN_EG, KING_EG };

// Contribución de cada tipo de pieza a la "fase" de la partida (máx. 24).
static const int PHASE_WEIGHT[6] = { 0, 1, 1, 2, 4, 0 };
static const int PHASE_MAX = 24;

// Espejo vertical para el bando negro (las tablas están en perspectiva blanca)
inline int mirror_sq(int sq) { return sq ^ 56; }

// ---------------------------------------------------------------------------
// Estructura de peones
// ---------------------------------------------------------------------------
struct PawnInfo {
    int count[2][8];      // peones por color y columna
    int most_adv[2][8];   // fila más avanzada (en perspectiva propia), -1 si no hay
    int least_adv[2][8];  // fila menos avanzada (perspectiva propia)
};

static void scan_pawns(const Board& b, PawnInfo& pi) {
    for (int c = 0; c < 2; c++)
        for (int f = 0; f < 8; f++) { pi.count[c][f] = 0; pi.most_adv[c][f] = -1; pi.least_adv[c][f] = 8; }
    for (int sq = 0; sq < 64; sq++) {
        Piece p = b.squares[sq];
        if (piece_type(p) != PAWN) continue;
        int c = piece_color(p), f = sq_file(sq);
        int rel = (c == WHITE) ? sq_rank(sq) : 7 - sq_rank(sq);
        pi.count[c][f]++;
        if (rel > pi.most_adv[c][f]) pi.most_adv[c][f] = rel;
        if (rel < pi.least_adv[c][f]) pi.least_adv[c][f] = rel;
    }
}

// Escudo de peones frente al rey enrocado. Penaliza por DISTANCIA real del
// peón al rey (v1.5 solo miraba si existía un peón en la columna, sin importar
// que estuviera en la otra punta del tablero).
static int king_shelter_penalty(Color us, int ksq, const PawnInfo& pi) {
    int penalty = 0;
    int kf = sq_file(ksq);
    int krel = (us == WHITE) ? sq_rank(ksq) : 7 - sq_rank(ksq);
    if (krel > 2) return 0;               // rey adelantado: no hay escudo que evaluar
    Color them = Color(us ^ 1);
    for (int df = -1; df <= 1; df++) {
        int f = kf + df;
        if (f < 0 || f > 7) continue;
        int own = pi.least_adv[us][f];    // peón propio más retrasado de esa columna
        if (own >= 8) {
            // Sin peón propio: columna abierta o semiabierta ante el rey.
            penalty += (df == 0) ? 27 : 17;
        } else {
            int dist = own - krel;        // 1 = peón justo delante del rey
            if (dist < 0) dist = 0;
            if (dist > 3) dist = 3;
            static const int SHIELD_PEN[4] = { 0, 0, 12, 22 };
            penalty += SHIELD_PEN[dist];
        }
        // Tormenta de peones enemigos: un peón enemigo cercano es peligroso.
        int enemy_adv = pi.most_adv[them][f];
        if (enemy_adv >= 0) {
            int enemy_rel_to_us = 7 - enemy_adv;      // fila del peón enemigo en perspectiva nuestra
            int d = enemy_rel_to_us - krel;
            if (d >= 0 && d <= 4) {
                static const int STORM[5] = { 24, 20, 14, 8, 3 };
                penalty += STORM[d];
            }
        } else {
            // Ningún peón enemigo en la columna: columna abierta hacia nuestro rey.
            penalty += 8;
        }
    }
    return penalty;
}

// Pesos de ataque por tipo de pieza para la zona del rey.
static const int KING_ATTACK_WEIGHT[6] = { 0, 20, 20, 40, 80, 0 };

// Presión sobre la zona del rey. v1.6 cuenta PIEZAS ATACANTES DISTINTAS
// ponderadas por tipo (lo que la documentación de v1.5 ya afirmaba), en vez de
// contar casillas atacadas incluyendo la casilla del propio rey.
static int king_zone_pressure(const Board& b, Color us, int ksq) {
    Color them = Color(us ^ 1);
    int kf = sq_file(ksq), kr = sq_rank(ksq);

    // Zona: las 8 casillas alrededor del rey (excluida la suya).
    Square zone[8]; int nz = 0;
    for (int df = -1; df <= 1; df++) for (int dr = -1; dr <= 1; dr++) {
        if (df == 0 && dr == 0) continue;      // v1.5 incluía la casilla del rey
        int f = kf + df, r = kr + dr;
        if (f < 0 || f > 7 || r < 0 || r > 7) continue;
        zone[nz++] = make_sq(f, r);
    }

    int attackers = 0, weight = 0;
    for (int sq = 0; sq < 64; sq++) {
        Piece p = b.squares[sq];
        if (piece_type(p) == NONE || piece_color(p) != them) continue;
        int pt = piece_type(p);
        if (pt == KING) continue;
        // ¿Esta pieza ataca alguna casilla de la zona?
        bool hits = false;
        for (int i = 0; i < nz && !hits; i++) {
            Square t = zone[i];
            int tf = sq_file(t), tr = sq_rank(t);
            int sf = sq_file(sq), sr = sq_rank(sq);
            int df = tf - sf, dr = tr - sr;
            switch (pt) {
                case PAWN: {
                    int pdir = (them == WHITE) ? 1 : -1;
                    if (dr == pdir && (df == 1 || df == -1)) hits = true;
                    break;
                }
                case KNIGHT: {
                    int a = df < 0 ? -df : df, c = dr < 0 ? -dr : dr;
                    if ((a == 1 && c == 2) || (a == 2 && c == 1)) hits = true;
                    break;
                }
                case BISHOP: case ROOK: case QUEEN: {
                    bool diag = (df == dr || df == -dr) && df != 0;
                    bool line = (df == 0 || dr == 0) && (df != 0 || dr != 0);
                    bool okdir = (pt == BISHOP) ? diag : (pt == ROOK) ? line : (diag || line);
                    if (!okdir) break;
                    int sx = (df > 0) - (df < 0), sy = (dr > 0) - (dr < 0);
                    int cf = sf + sx, cr2 = sr + sy;
                    bool blocked = false;
                    while (cf != tf || cr2 != tr) {
                        if (piece_type(b.squares[make_sq(cf, cr2)]) != NONE) { blocked = true; break; }
                        cf += sx; cr2 += sy;
                    }
                    if (!blocked) hits = true;
                    break;
                }
                default: break;
            }
        }
        if (hits) { attackers++; weight += KING_ATTACK_WEIGHT[pt]; }
    }

    if (attackers < 2) return 0;   // una pieza sola no genera un ataque real
    // Escala no lineal sobre el peso acumulado.
    int idx = attackers > 7 ? 7 : attackers;
    static const int SCALE[8] = { 0, 0, 50, 75, 88, 94, 97, 99 };
    return weight * SCALE[idx] / 100;
}

// ---------------------------------------------------------------------------
// Evaluación principal
// ---------------------------------------------------------------------------
Score evaluate(const Board& b) {
    int mg = 0, eg = 0;
    int phase = 0;
    int bishops[2] = {0, 0};
    int ksq[2] = {-1, -1};
    int rooks[2][8];                     // torres por columna (para columnas abiertas)
    for (int c = 0; c < 2; c++) for (int f = 0; f < 8; f++) rooks[c][f] = 0;

    PawnInfo pi;
    scan_pawns(b, pi);

    // ---- Barrido único: material + PST + inventario ----
    for (int sq = 0; sq < 64; sq++) {
        Piece p = b.squares[sq];
        if (piece_type(p) == NONE) continue;
        int pt = piece_type(p);
        Color c = piece_color(p);
        phase += PHASE_WEIGHT[pt];
        int idx = (c == WHITE) ? sq : mirror_sq(sq);
        int vmg = MAT_MG[pt] + PST_MG[pt][idx];
        int veg = MAT_EG[pt] + PST_EG[pt][idx];
        if (c == WHITE) { mg += vmg; eg += veg; } else { mg -= vmg; eg -= veg; }
        if (pt == BISHOP) bishops[c]++;
        if (pt == KING)   ksq[c] = sq;
        if (pt == ROOK)   rooks[c][sq_file(sq)]++;
    }

    // ---- Pareja de alfiles (escalada por fase: vale más en el final) ----
    if (bishops[WHITE] >= 2) { mg += 30; eg += 55; }
    if (bishops[BLACK] >= 2) { mg -= 30; eg -= 55; }

    // ---- Estructura de peones ----
    // Bonos de peón pasado por fila relativa (0 = fila inicial, 6 = 7ª fila).
    static const int PASSED_MG[8] = { 0,  5, 10, 20, 35, 60,  95, 0 };
    static const int PASSED_EG[8] = { 0, 15, 25, 45, 75, 120, 175, 0 };

    for (int c = 0; c < 2; c++) {
        Color us = Color(c), them = Color(c ^ 1);
        int sign = (us == WHITE) ? 1 : -1;
        for (int f = 0; f < 8; f++) {
            int n = pi.count[us][f];
            if (n == 0) continue;
            // Doblados
            if (n > 1) { mg -= sign * 12 * (n - 1); eg -= sign * 24 * (n - 1); }
            // Aislados
            bool neigh = (f > 0 && pi.count[us][f-1] > 0) || (f < 7 && pi.count[us][f+1] > 0);
            if (!neigh) { mg -= sign * 16; eg -= sign * 22; }
        }
        // Peones pasados: se evalúa CADA peón, no solo el más avanzado (v1.5).
        for (int sq = 0; sq < 64; sq++) {
            Piece p = b.squares[sq];
            if (piece_type(p) != PAWN || piece_color(p) != us) continue;
            int f = sq_file(sq);
            int rel = (us == WHITE) ? sq_rank(sq) : 7 - sq_rank(sq);
            bool passed = true;
            for (int df = -1; df <= 1 && passed; df++) {
                int ff = f + df;
                if (ff < 0 || ff > 7) continue;
                int en = pi.least_adv[them][ff];
                if (en < 0) continue;
                // El peón enemigo MENOS avanzado de esa columna (el que ocupa
                // la fila más alta en nuestra perspectiva) es el que primero
                // nos frena. Si ese aún está por delante, no somos pasados (un
                // peón más retrasado detrás no cuenta). Usar most_adv declaraba
                // pasados peones que en realidad tenían un enemigo por delante.
                if ((7 - en) > rel) passed = false;
            }
            if (passed) {
                mg += sign * PASSED_MG[rel];
                eg += sign * PASSED_EG[rel];
                // Peón pasado protegido por otro peón: más valioso aún.
                int pdir = (us == WHITE) ? -1 : 1;
                int br = sq_rank(sq) + pdir;
                if (br >= 0 && br < 8) {
                    for (int df : {-1, 1}) {
                        int bf = f + df;
                        if (bf < 0 || bf > 7) continue;
                        if (b.squares[make_sq(bf, br)] == make_piece(us, PAWN)) {
                            mg += sign * 12; eg += sign * 20; break;
                        }
                    }
                }
            }
        }
    }

    // ---- Torres en columna abierta / semiabierta ----
    for (int c = 0; c < 2; c++) {
        Color us = Color(c), them = Color(c ^ 1);
        int sign = (us == WHITE) ? 1 : -1;
        for (int f = 0; f < 8; f++) {
            if (!rooks[us][f]) continue;
            bool own_pawn   = pi.count[us][f]   > 0;
            bool enemy_pawn = pi.count[them][f] > 0;
            if (!own_pawn && !enemy_pawn)      { mg += sign * 30 * rooks[us][f]; eg += sign * 12 * rooks[us][f]; }
            else if (!own_pawn)                { mg += sign * 15 * rooks[us][f]; eg += sign *  6 * rooks[us][f]; }
        }
        // Torre en 7ª fila (relativa): presiona los peones y confina al rey.
        for (int sq = 0; sq < 64; sq++) {
            Piece p = b.squares[sq];
            if (piece_type(p) != ROOK || piece_color(p) != us) continue;
            int rel = (us == WHITE) ? sq_rank(sq) : 7 - sq_rank(sq);
            if (rel == 6) { mg += sign * 20; eg += sign * 32; }
        }
    }

    // ---- Seguridad del rey (medio juego) ----
    for (int c = 0; c < 2; c++) {
        Color us = Color(c);
        if (ksq[us] < 0) continue;
        int sign = (us == WHITE) ? 1 : -1;
        int pen = king_shelter_penalty(us, ksq[us], pi)
                + king_zone_pressure(b, us, ksq[us]);
        mg -= sign * pen;
    }

    // ---- Movilidad de piezas (sin peones ni rey, sin casillas batidas por peones) ----
    int mob_w = piece_mobility(b, WHITE);
    int mob_b = piece_mobility(b, BLACK);
    mg += 4 * (mob_w - mob_b);
    eg += 3 * (mob_w - mob_b);

    // ---- Tempo ----
    int tempo = (b.side == WHITE) ? 12 : -12;
    mg += tempo;

    // ---- Interpolación por fases (tapered eval) ----
    if (phase > PHASE_MAX) phase = PHASE_MAX;
    int score = (mg * phase + eg * (PHASE_MAX - phase)) / PHASE_MAX;

    return (b.side == WHITE) ? score : -score;
}

} // namespace hy3
