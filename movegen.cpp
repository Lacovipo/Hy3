// movegen.cpp - Implementación del generador de movimientos
#include "movegen.h"

namespace hy3 {

namespace {

void add_pawn_move(MoveList& moves, Square from, Square to, int flag) {
    if (sq_rank(to) == 7 || sq_rank(to) == 0) {
        // Promociones
        moves.push_back(make_move(from, to, 1, flag)); // knight
        moves.push_back(make_move(from, to, 2, flag)); // bishop
        moves.push_back(make_move(from, to, 3, flag)); // rook
        moves.push_back(make_move(from, to, 4, flag)); // queen
    } else {
        moves.push_back(make_move(from, to, 0, flag));
    }
}

} // namespace

// Cuenta movimientos pseudo-legales del bando b.side sin construir un vector
// (evita asignaciones en caminos calientes como quiescence/eval).
int count_pseudo_moves(const Board& b) {
    return count_pseudo_moves(b, b.side);
}

int count_pseudo_moves(const Board& b, Color us) {
    Color them = Color(us ^ 1);
    int dir = (us == WHITE) ? 1 : -1;
    int count = 0;

    for (int sq = 0; sq < N_SQ; sq++) {
        Piece p = b.squares[sq];
        if (piece_type(p) == NONE) continue;
        if (piece_color(p) != us) continue;
        int f = sq_file(sq), r = sq_rank(sq);

        switch (piece_type(p)) {
            case PAWN: {
                int nr = r + dir;
                if (nr >= 0 && nr < 8) {
                    Square to = make_sq(f, nr);
                    if (piece_type(b.squares[to]) == NONE) {
                        count++;
                        int nr2 = r + 2 * dir;
                        if ((us == WHITE && r == 1) || (us == BLACK && r == 6)) {
                            Square to2 = make_sq(f, nr2);
                            if (piece_type(b.squares[to2]) == NONE) count++;
                        }
                    }
                }
                for (int df : {-1, 1}) {
                    int cf = f + df, cr = r + dir;
                    if (cf < 0 || cf > 7 || cr < 0 || cr > 7) continue;
                    Square to = make_sq(cf, cr);
                    Piece tgt = b.squares[to];
                    if (piece_type(tgt) != NONE && piece_color(tgt) == them) count++;
                    else if (to == b.ep_square) count++;
                }
                break;
            }
            case KNIGHT: {
                static const int kn[8][2] = {{1,2},{2,1},{2,-1},{1,-2},{-1,-2},{-2,-1},{-2,1},{-1,2}};
                for (auto& d : kn) {
                    int nf = f + d[0], nr = r + d[1];
                    if (nf<0||nf>7||nr<0||nr>7) continue;
                    Square to = make_sq(nf, nr);
                    Piece tgt = b.squares[to];
                    if (piece_type(tgt) == NONE || piece_color(tgt) == them) count++;
                }
                break;
            }
            case BISHOP:
            case ROOK:
            case QUEEN: {
                static const int di[4][2] = {{1,1},{1,-1},{-1,1},{-1,-1}};
                static const int li[4][2] = {{1,0},{-1,0},{0,1},{0,-1}};
                static const int qu[8][2] = {{1,1},{1,-1},{-1,1},{-1,-1},{1,0},{-1,0},{0,1},{0,-1}};
                const int (*dirs)[2] = nullptr;
                int ndirs = 0;
                if (piece_type(p) == BISHOP) { dirs = di; ndirs = 4; }
                else if (piece_type(p) == ROOK) { dirs = li; ndirs = 4; }
                else { dirs = qu; ndirs = 8; }
                for (int i = 0; i < ndirs; i++) {
                    int nf = f + dirs[i][0], nr = r + dirs[i][1];
                    while (nf>=0&&nf<8&&nr>=0&&nr<8) {
                        Square to = make_sq(nf, nr);
                        Piece tgt = b.squares[to];
                        if (piece_type(tgt) == NONE) count++;
                        else {
                            if (piece_color(tgt) == them) count++;
                            break;
                        }
                        nf += dirs[i][0]; nr += dirs[i][1];
                    }
                }
                break;
            }
            case KING: {
                for (int df=-1; df<=1; df++) for (int dr=-1; dr<=1; dr++) {
                    if (df==0&&dr==0) continue;
                    int nf=f+df, nr=r+dr;
                    if (nf<0||nf>7||nr<0||nr>7) continue;
                    Square to = make_sq(nf, nr);
                    Piece tgt = b.squares[to];
                    if (piece_type(tgt)==NONE || piece_color(tgt)==them) count++;
                }
                if (us == WHITE) {
                    if ((b.castling & 1) && b.squares[make_sq(5,0)]==make_piece(WHITE,NONE)
                        && b.squares[make_sq(6,0)]==make_piece(WHITE,NONE)
                        && b.squares[make_sq(7,0)]==make_piece(WHITE,ROOK)
                        && !b.is_square_attacked(make_sq(4,0), them)
                        && !b.is_square_attacked(make_sq(5,0), them)
                        && !b.is_square_attacked(make_sq(6,0), them)) count++;
                    if ((b.castling & 2) && b.squares[make_sq(3,0)]==make_piece(WHITE,NONE)
                        && b.squares[make_sq(2,0)]==make_piece(WHITE,NONE)
                        && b.squares[make_sq(1,0)]==make_piece(WHITE,NONE)
                        && b.squares[make_sq(0,0)]==make_piece(WHITE,ROOK)
                        && !b.is_square_attacked(make_sq(4,0), them)
                        && !b.is_square_attacked(make_sq(3,0), them)
                        && !b.is_square_attacked(make_sq(2,0), them)) count++;
                } else {
                    if ((b.castling & 4) && b.squares[make_sq(5,7)]==make_piece(WHITE,NONE)
                        && b.squares[make_sq(6,7)]==make_piece(WHITE,NONE)
                        && b.squares[make_sq(7,7)]==make_piece(BLACK,ROOK)
                        && !b.is_square_attacked(make_sq(4,7), them)
                        && !b.is_square_attacked(make_sq(5,7), them)
                        && !b.is_square_attacked(make_sq(6,7), them)) count++;
                    if ((b.castling & 8) && b.squares[make_sq(3,7)]==make_piece(WHITE,NONE)
                        && b.squares[make_sq(2,7)]==make_piece(WHITE,NONE)
                        && b.squares[make_sq(1,7)]==make_piece(WHITE,NONE)
                        && b.squares[make_sq(0,7)]==make_piece(BLACK,ROOK)
                        && !b.is_square_attacked(make_sq(4,7), them)
                        && !b.is_square_attacked(make_sq(3,7), them)
                        && !b.is_square_attacked(make_sq(2,7), them)) count++;
                }
                break;
            }
            default: break;
        }
    }
    return count;
}

void generate_pseudo_moves(const Board& b, MoveList& moves) {
    moves.clear();
    Color us = b.side;
    Color them = Color(us ^ 1);
    int dir = (us == WHITE) ? 1 : -1; // dirección de avance de peones

    for (int sq = 0; sq < N_SQ; sq++) {
        Piece p = b.squares[sq];
        if (piece_type(p) == NONE) continue;
        if (piece_color(p) != us) continue;
        int f = sq_file(sq), r = sq_rank(sq);

        switch (piece_type(p)) {
            case PAWN: {
                // Avance simple
                int nr = r + dir;
                if (nr >= 0 && nr < 8) {
                    Square to = make_sq(f, nr);
                    if (piece_type(b.squares[to]) == NONE) {
                        add_pawn_move(moves, sq, to, FLAG_NORMAL);
                        // Avance doble
                        int nr2 = r + 2 * dir;
                        if ((us == WHITE && r == 1) || (us == BLACK && r == 6)) {
                            Square to2 = make_sq(f, nr2);
                            if (piece_type(b.squares[to2]) == NONE)
                                moves.push_back(make_move(sq, to2, 0, FLAG_NORMAL));
                        }
                    }
                }
                // Capturas
                for (int df : {-1, 1}) {
                    int cf = f + df, cr = r + dir;
                    if (cf < 0 || cf > 7 || cr < 0 || cr > 7) continue;
                    Square to = make_sq(cf, cr);
                    Piece tgt = b.squares[to];
                    if (piece_type(tgt) != NONE && piece_color(tgt) == them) {
                        add_pawn_move(moves, sq, to, FLAG_CAPTURE);
                    } else if (to == b.ep_square) {
                        add_pawn_move(moves, sq, to, FLAG_EP);
                    }
                }
                break;
            }
            case KNIGHT: {
                static const int kn[8][2] = {{1,2},{2,1},{2,-1},{1,-2},{-1,-2},{-2,-1},{-2,1},{-1,2}};
                for (auto& d : kn) {
                    int nf = f + d[0], nr = r + d[1];
                    if (nf<0||nf>7||nr<0||nr>7) continue;
                    Square to = make_sq(nf, nr);
                    Piece tgt = b.squares[to];
                    if (piece_type(tgt) == NONE || piece_color(tgt) == them)
                        moves.push_back(make_move(sq, to, 0, piece_type(tgt)!=NONE?FLAG_CAPTURE:FLAG_NORMAL));
                }
                break;
            }
            case BISHOP:
            case ROOK:
            case QUEEN: {
                // Direcciones según tipo
                static const int di[4][2] = {{1,1},{1,-1},{-1,1},{-1,-1}};
                static const int li[4][2] = {{1,0},{-1,0},{0,1},{0,-1}};
                // Reina: 8 direcciones (diagonales + rectas)
                static const int qu[8][2] = {{1,1},{1,-1},{-1,1},{-1,-1},{1,0},{-1,0},{0,1},{0,-1}};
                const int (*dirs)[2] = nullptr;
                int ndirs = 0;
                if (piece_type(p) == BISHOP) { dirs = di; ndirs = 4; }
                else if (piece_type(p) == ROOK) { dirs = li; ndirs = 4; }
                else { dirs = qu; ndirs = 8; } // reina: 8 direcciones
                for (int i = 0; i < ndirs; i++) {
                    int nf = f + dirs[i][0], nr = r + dirs[i][1];
                    while (nf>=0&&nf<8&&nr>=0&&nr<8) {
                        Square to = make_sq(nf, nr);
                        Piece tgt = b.squares[to];
                        if (piece_type(tgt) == NONE) {
                            moves.push_back(make_move(sq, to, 0, FLAG_NORMAL));
                        } else {
                            if (piece_color(tgt) == them)
                                moves.push_back(make_move(sq, to, 0, FLAG_CAPTURE));
                            break;
                        }
                        nf += dirs[i][0]; nr += dirs[i][1];
                    }
                }
                break;
            }
            case KING: {
                for (int df=-1; df<=1; df++) for (int dr=-1; dr<=1; dr++) {
                    if (df==0&&dr==0) continue;
                    int nf=f+df, nr=r+dr;
                    if (nf<0||nf>7||nr<0||nr>7) continue;
                    Square to = make_sq(nf, nr);
                    Piece tgt = b.squares[to];
                    if (piece_type(tgt)==NONE || piece_color(tgt)==them)
                        moves.push_back(make_move(sq, to, 0, piece_type(tgt)!=NONE?FLAG_CAPTURE:FLAG_NORMAL));
                }
                // Enroque
                if (us == WHITE) {
                    if ((b.castling & 1) && b.squares[make_sq(5,0)]==make_piece(WHITE,NONE)
                        && b.squares[make_sq(6,0)]==make_piece(WHITE,NONE)
                        && b.squares[make_sq(7,0)]==make_piece(WHITE,ROOK)
                        && !b.is_square_attacked(make_sq(4,0), them)
                        && !b.is_square_attacked(make_sq(5,0), them)
                        && !b.is_square_attacked(make_sq(6,0), them))
                        moves.push_back(make_move(sq, make_sq(6,0), 0, FLAG_CASTLE));
                    if ((b.castling & 2) && b.squares[make_sq(3,0)]==make_piece(WHITE,NONE)
                        && b.squares[make_sq(2,0)]==make_piece(WHITE,NONE)
                        && b.squares[make_sq(1,0)]==make_piece(WHITE,NONE)
                        && b.squares[make_sq(0,0)]==make_piece(WHITE,ROOK)
                        && !b.is_square_attacked(make_sq(4,0), them)
                        && !b.is_square_attacked(make_sq(3,0), them)
                        && !b.is_square_attacked(make_sq(2,0), them))
                        moves.push_back(make_move(sq, make_sq(2,0), 0, FLAG_CASTLE));
                } else {
                    if ((b.castling & 4) && b.squares[make_sq(5,7)]==make_piece(WHITE,NONE)
                        && b.squares[make_sq(6,7)]==make_piece(WHITE,NONE)
                        && b.squares[make_sq(7,7)]==make_piece(BLACK,ROOK)
                        && !b.is_square_attacked(make_sq(4,7), them)
                        && !b.is_square_attacked(make_sq(5,7), them)
                        && !b.is_square_attacked(make_sq(6,7), them))
                        moves.push_back(make_move(sq, make_sq(6,7), 0, FLAG_CASTLE));
                    if ((b.castling & 8) && b.squares[make_sq(3,7)]==make_piece(WHITE,NONE)
                        && b.squares[make_sq(2,7)]==make_piece(WHITE,NONE)
                        && b.squares[make_sq(1,7)]==make_piece(WHITE,NONE)
                        && b.squares[make_sq(0,7)]==make_piece(BLACK,ROOK)
                        && !b.is_square_attacked(make_sq(4,7), them)
                        && !b.is_square_attacked(make_sq(3,7), them)
                        && !b.is_square_attacked(make_sq(2,7), them))
                        moves.push_back(make_move(sq, make_sq(2,7), 0, FLAG_CASTLE));
                }
                break;
            }
            default: break;
        }
    }
}

std::vector<Move> generate_pseudo_moves(const Board& b) {
    MoveList ml;
    generate_pseudo_moves(b, ml);
    return std::vector<Move>(ml.begin(), ml.end());
}

} // namespace hy3