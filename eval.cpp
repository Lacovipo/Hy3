// eval.cpp - Implementación de la evaluación
#include "eval.h"

namespace hy3 {

const int PIECE_VALUE[6] = { 100, 320, 330, 500, 900, 20000 };

// Piece-square tables (perspectiva blanca, índice = sq donde a1=0, b1=1, ..., h8=63).
// Peones
static const int PAWN_PST[64] = {
     0,  0,  0,  0,  0,  0,  0,  0,
     5, 10, 10,-20,-20, 10, 10,  5,
     5, -5,-10,  0,  0,-10, -5,  5,
     0,  0,  0, 20, 20,  0,  0,  0,
     5,  5, 10, 25, 25, 10,  5,  5,
    10, 10, 20, 30, 30, 20, 10, 10,
    50, 50, 50, 50, 50, 50, 50, 50,
     0,  0,  0,  0,  0,  0,  0,  0
};
static const int KNIGHT_PST[64] = {
    -50,-40,-30,-30,-30,-30,-40,-50,
    -40,-20,  0,  5,  5,  0,-20,-40,
    -30,  5, 10, 15, 15, 10,  5,-30,
    -30,  0, 15, 20, 20, 15,  0,-30,
    -30,  5, 15, 20, 20, 15,  5,-30,
    -30,  0, 10, 15, 15, 10,  0,-30,
    -40,-20,  0,  0,  0,  0,-20,-40,
    -50,-40,-30,-30,-30,-30,-40,-50
};
static const int BISHOP_PST[64] = {
    -20,-10,-10,-10,-10,-10,-10,-20,
    -10,  5,  0,  0,  0,  0,  5,-10,
    -10, 10, 10, 10, 10, 10, 10,-10,
    -10,  0, 10, 10, 10, 10,  0,-10,
    -10,  5,  5, 10, 10,  5,  5,-10,
    -10,  0,  5, 10, 10,  5,  0,-10,
    -10,  0,  0,  0,  0,  0,  0,-10,
    -20,-10,-10,-10,-10,-10,-10,-20
};
static const int ROOK_PST[64] = {
     0,  0,  0,  5,  5,  0,  0,  0,
    -5,  0,  0,  0,  0,  0,  0, -5,
    -5,  0,  0,  0,  0,  0,  0, -5,
    -5,  0,  0,  0,  0,  0,  0, -5,
    -5,  0,  0,  0,  0,  0,  0, -5,
    -5,  0,  0,  0,  0,  0,  0, -5,
     5, 10, 10, 10, 10, 10, 10,  5,
     0,  0,  0,  0,  0,  0,  0,  0
};
static const int QUEEN_PST[64] = {
    -20,-10,-10, -5, -5,-10,-10,-20,
    -10,  0,  5,  0,  0,  0,  0,-10,
    -10,  5,  5,  5,  5,  5,  0,-10,
      0,  0,  5,  5,  5,  5,  0, -5,
     -5,  0,  5,  5,  5,  5,  0, -5,
    -10,  0,  5,  5,  5,  5,  0,-10,
    -10,  0,  0,  0,  0,  0,  0,-10,
    -20,-10,-10, -5, -5,-10,-10,-20
};
// Perspectiva blanca, índice 0 = a1: bonos de enroque en filas 1-2,
// penalizaciones crecientes hacia el campo enemigo.
static const int KING_PST[64] = {
     20, 30, 10,  0,  0, 10, 30, 20,
     20, 20,  0,  0,  0,  0, 20, 20,
    -10,-20,-20,-20,-20,-20,-20,-10,
    -20,-30,-30,-40,-40,-30,-30,-20,
    -30,-40,-40,-50,-50,-40,-40,-30,
    -30,-40,-40,-50,-50,-40,-40,-30,
    -30,-40,-40,-50,-50,-40,-40,-30,
    -30,-40,-40,-50,-50,-40,-40,-30
};
// Tabla de rey para el FINAL (EG): el rey debe activarse hacia el centro.
static const int KING_EG_PST[64] = {
    -50,-30,-30,-30,-30,-30,-30,-50,
    -30,-30,  0,  0,  0,  0,-30,-30,
    -30,-10, 20, 30, 30, 20,-10,-30,
    -30,-10, 30, 40, 40, 30,-10,-30,
    -30,-10, 30, 40, 40, 30,-10,-30,
    -30,-10, 20, 30, 30, 20,-10,-30,
    -30,-20,-10,  0,  0,-10,-20,-30,
    -50,-40,-30,-20,-20,-30,-40,-50
};

static const int* PST[6] = { PAWN_PST, KNIGHT_PST, BISHOP_PST, ROOK_PST, QUEEN_PST, KING_PST };

// Contribución de cada tipo de pieza a la "fase" de la partida. La fase total
// máxima (posición inicial) es 24; disminuye a medida que se cambian piezas.
static const int PHASE_WEIGHT[6] = { 0, 1, 1, 2, 4, 0 }; // P,N,B,R,Q,K
static const int PHASE_MAX = 24;

// Espejo vertical para el bando negro (la tabla está en perspectiva blanca)
inline int mirror_sq(int sq) { return sq ^ 56; } // invierte el rango

static int mobility(const Board& b, Color us) {
    // Movilidad = nº de movimientos pseudolegales del bando 'us'
    // (sin copiar el tablero: camino muy caliente, se llama 2x por evaluate()).
    return count_pseudo_moves(b, us);
}

// Cuenta peones por columna para cada color (columnas a..h = 0..7).
static void pawn_files(const Board& b, int wp[8], int bp[8]) {
    for (int i = 0; i < 8; i++) { wp[i] = 0; bp[i] = 0; }
    for (int sq = 0; sq < 64; sq++) {
        Piece p = b.squares[sq];
        if (piece_type(p) != PAWN) continue;
        int f = sq_file(sq);
        if (piece_color(p) == WHITE) wp[f]++;
        else bp[f]++;
    }
}

// Cuenta escudo de peones y semiapertura frente al rey enrocado.
// Devuelve una penalización (>=0) que se restará al bando propietario del rey.
static int king_shelter_penalty(const Board& b, Color us, int ksq,
                                const int wp[8], const int bp[8]) {
    int penalty = 0;
    int kf = sq_file(ksq);
    int kr = sq_rank(ksq);
    const int* own = (us == WHITE) ? wp : bp;
    const int* enemy = (us == WHITE) ? bp : wp;
    // Solo evaluamos escudo si el rey está en su propio campo (enrocado o cerca).
    bool home = (us == WHITE) ? (kr <= 1) : (kr >= 6);
    if (home) {
        for (int df = -1; df <= 1; df++) {
            int f = kf + df;
            if (f < 0 || f > 7) continue;
            // Falta de peón propio en la columna del escudo.
            if (own[f] == 0) penalty += (df == 0) ? 25 : 15;
            // Columna abierta/semiabierta frente al rey (sin peón enemigo).
            if (enemy[f] == 0) penalty += 10;
        }
    }
    return penalty;
}

// Zona de ataque del rey: cuenta piezas enemigas que atacan las 8 casillas
// alrededor del rey y aplica una escala no lineal.
static int king_zone_pressure(const Board& b, Color us, int ksq) {
    Color them = Color(us ^ 1);
    int kf = sq_file(ksq), kr = sq_rank(ksq);
    int attackers = 0;
    for (int df = -1; df <= 1; df++) for (int dr = -1; dr <= 1; dr++) {
        int f = kf + df, r = kr + dr;
        if (f < 0 || f > 7 || r < 0 || r > 7) continue;
        if (b.is_square_attacked(make_sq(f, r), them)) attackers++;
    }
    // Escala no lineal: cada atacante adicional pesa más.
    static const int scale[10] = { 0, 2, 6, 12, 20, 30, 42, 56, 72, 90 };
    int idx = attackers > 9 ? 9 : attackers;
    return scale[idx];
}

Score evaluate(const Board& b) {
    int mg = 0;   // acumulador medio juego
    int eg = 0;   // acumulador final
    int phase = 0;
    int wbishops = 0, bbishops = 0;
    int wk = -1, bk = -1;
    int wp[8], bp[8];
    pawn_files(b, wp, bp);

    // Peones por fila para detectar peones pasados / avanzados.
    int wr[8], br[8];
    for (int i = 0; i < 8; i++) { wr[i] = -1; br[i] = -1; }

    for (int sq = 0; sq < 64; sq++) {
        Piece p = b.squares[sq];
        if (piece_type(p) == NONE) continue;
        int pt = piece_type(p);
        int val = PIECE_VALUE[pt];
        phase += PHASE_WEIGHT[pt];
        if (piece_color(p) == WHITE) {
            // El rey aporta su PST interpolada aparte; el resto usa PST única.
            if (pt == KING) {
                mg += val + KING_PST[sq];
                eg += val + KING_EG_PST[sq];
                wk = sq;
            } else {
                mg += val + PST[pt][sq];
                eg += val + PST[pt][sq];
            }
            if (pt == BISHOP) wbishops++;
            if (pt == PAWN) { int r = sq_rank(sq); if (r > wr[sq_file(sq)]) wr[sq_file(sq)] = r; }
        } else {
            int m = mirror_sq(sq);
            if (pt == KING) {
                mg -= val + KING_PST[m];
                eg -= val + KING_EG_PST[m];
                bk = sq;
            } else {
                mg -= val + PST[pt][m];
                eg -= val + PST[pt][m];
            }
            if (pt == BISHOP) bbishops++;
            if (pt == PAWN) { int r = 7 - sq_rank(sq); if (r > br[sq_file(sq)]) br[sq_file(sq)] = r; }
        }
    }

    // Par de alfiles (igual en ambas fases).
    if (wbishops >= 2) { mg += 30; eg += 30; }
    if (bbishops >= 2) { mg -= 30; eg -= 30; }

    // Estructura de peones: penaliza peones doblados y aislados; premia pasados.
    for (int f = 0; f < 8; f++) {
        if (wp[f] > 1) { mg -= 12 * (wp[f] - 1); eg -= 16 * (wp[f] - 1); }
        if (bp[f] > 1) { mg += 12 * (bp[f] - 1); eg += 16 * (bp[f] - 1); }
        if (wp[f] > 0) {
            bool neigh = (f > 0 && wp[f-1] > 0) || (f < 7 && wp[f+1] > 0);
            if (!neigh) { mg -= 14; eg -= 18; }
            bool passed = true;
            for (int df = -1; df <= 1; df++) {
                int ff = f + df; if (ff < 0 || ff > 7) continue;
                for (int r = wr[f] + 1; r < 8; r++)
                    if (b.squares[make_sq(ff, r)] == make_piece(BLACK, PAWN)) { passed = false; break; }
                if (!passed) break;
            }
            if (passed) {
                int rank = wr[f];
                mg += 10 + 6 * rank + (rank >= 5 ? 10 : 0);
                eg += 20 + 12 * rank + (rank >= 5 ? 20 : 0);  // pasados valen más en el final
            }
        }
        if (bp[f] > 0) {
            bool neigh = (f > 0 && bp[f-1] > 0) || (f < 7 && bp[f+1] > 0);
            if (!neigh) { mg += 14; eg += 18; }
            bool passed = true;
            int lead_rank = 7 - br[f];
            for (int df = -1; df <= 1; df++) {
                int ff = f + df; if (ff < 0 || ff > 7) continue;
                for (int r = lead_rank - 1; r >= 0; r--)
                    if (b.squares[make_sq(ff, r)] == make_piece(WHITE, PAWN)) { passed = false; break; }
                if (!passed) break;
            }
            if (passed) {
                int rank = br[f];
                mg -= 10 + 6 * rank + (rank >= 5 ? 10 : 0);
                eg -= 20 + 12 * rank + (rank >= 5 ? 20 : 0);
            }
        }
    }

    // ---- Seguridad del rey (solo en medio juego: en el final el rey es activo) ----
    if (wk >= 0) {
        int pen = king_shelter_penalty(b, WHITE, wk, wp, bp) + king_zone_pressure(b, WHITE, wk);
        mg -= pen;
    }
    if (bk >= 0) {
        int pen = king_shelter_penalty(b, BLACK, bk, wp, bp) + king_zone_pressure(b, BLACK, bk);
        mg += pen;
    }

    // Movilidad: (mis movimientos) - (movimientos del rival)
    int my = mobility(b, b.side);
    int op = mobility(b, Color(b.side ^ 1));
    int mob = my - op;
    // La movilidad se mide desde la perspectiva del lado en turno; la aplicamos
    // a ambas fases con signo relativo a las blancas.
    int mob_white = (b.side == WHITE) ? mob : -mob;
    mg += 2 * mob_white;
    eg += 1 * mob_white;

    // Tempo: ligero incentivo a mover.
    int tempo_white = (b.side == WHITE) ? 10 : -10;
    mg += tempo_white;

    // ---- Interpolación por fases (Tapered Eval) ----
    if (phase > PHASE_MAX) phase = PHASE_MAX;
    int score = (mg * phase + eg * (PHASE_MAX - phase)) / PHASE_MAX;

    // Devolvemos desde la perspectiva del lado que mueve.
    return (b.side == WHITE) ? score : -score;
}

} // namespace hy3
