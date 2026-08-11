// tests/verify.cpp - Generador naive independiente para aislar bugs del movegen
#include "../src/board.h"
#include "../src/perft.h"
#include <iostream>
#include <algorithm>
#include <set>
using namespace hy3;

// Generador naive: para cada (from,to) comprueba geometria y filtra con is_legal.
// Reimplementa la geometria de forma independiente al movegen rapido.
namespace {

bool path_clear(const Board& b, Square a, Square c) {
    // asume a y c alineados; devuelve true si no hay pieza entre medias (excluye extremos)
    int fa = sq_file(a), ra = sq_rank(a), fc = sq_file(c), rc = sq_rank(c);
    int df = fc - fa, dr = rc - ra;
    int sgnf = (df > 0) ? 1 : (df < 0 ? -1 : 0);
    int sgnr = (dr > 0) ? 1 : (dr < 0 ? -1 : 0);
    int f = fa + sgnf, r = ra + sgnr;
    while (f != fc || r != rc) {
        if (piece_type(b.squares[make_sq(f, r)]) != NONE) return false;
        f += sgnf; r += sgnr;
    }
    return true;
}

std::vector<Move> naive_pseudo(const Board& b) {
    std::vector<Move> out;
    Color us = b.side;
    Color them = Color(us ^ 1);
    int dir = (us == WHITE) ? 1 : -1;
    for (int from = 0; from < 64; from++) {
        Piece p = b.squares[from];
        if (piece_type(p) == NONE || piece_color(p) != us) continue;
        int ff = sq_file(from), fr = sq_rank(from);
        PieceType pt = piece_type(p);
        if (pt == PAWN) {
            // single push
            int nr = fr + dir;
            if (nr >= 0 && nr < 8) {
                Square to = make_sq(ff, nr);
                if (piece_type(b.squares[to]) == NONE) {
                    if (nr == 7 || nr == 0) {
                        for (int pr = 1; pr <= 4; pr++) out.push_back(make_move(from, to, pr, FLAG_NORMAL));
                    } else out.push_back(make_move(from, to, 0, FLAG_NORMAL));
                    // double push
                    if ((us == WHITE && fr == 1) || (us == BLACK && fr == 6)) {
                        int nr2 = fr + 2 * dir;
                        Square to2 = make_sq(ff, nr2);
                        if (piece_type(b.squares[to2]) == NONE)
                            out.push_back(make_move(from, to2, 0, FLAG_NORMAL));
                    }
                }
            }
            // captures
            for (int df : {-1, 1}) {
                int cf = ff + df, cr = fr + dir;
                if (cf < 0 || cf > 7 || cr < 0 || cr > 7) continue;
                Square to = make_sq(cf, cr);
                Piece tgt = b.squares[to];
                int flag = FLAG_NORMAL;
                if (piece_type(tgt) != NONE && piece_color(tgt) == them) flag = FLAG_CAPTURE;
                else if (to == b.ep_square) flag = FLAG_EP;
                else continue;
                if (cr == 7 || cr == 0) {
                    for (int pr = 1; pr <= 4; pr++) out.push_back(make_move(from, to, pr, flag));
                } else out.push_back(make_move(from, to, 0, flag));
            }
            continue;
        }
        if (pt == KNIGHT) {
            static const int kn[8][2] = {{1,2},{2,1},{2,-1},{1,-2},{-1,-2},{-2,-1},{-2,1},{-1,2}};
            for (auto& d : kn) {
                int nf = ff + d[0], nr = fr + d[1];
                if (nf<0||nf>7||nr<0||nr>7) continue;
                Square to = make_sq(nf, nr);
                Piece tgt = b.squares[to];
                if (piece_type(tgt) == NONE) out.push_back(make_move(from, to, 0, FLAG_NORMAL));
                else if (piece_color(tgt) == them) out.push_back(make_move(from, to, 0, FLAG_CAPTURE));
            }
            continue;
        }
        if (pt == KING) {
            for (int df=-1; df<=1; df++) for (int dr=-1; dr<=1; dr++) {
                if (df==0&&dr==0) continue;
                int nf=ff+df, nr=fr+dr;
                if (nf<0||nf>7||nr<0||nr>7) continue;
                Square to = make_sq(nf, nr);
                Piece tgt = b.squares[to];
                if (piece_type(tgt)==NONE) out.push_back(make_move(from, to, 0, FLAG_NORMAL));
                else if (piece_color(tgt)==them) out.push_back(make_move(from, to, 0, FLAG_CAPTURE));
            }
            // castling
            if (us == WHITE) {
                if ((b.castling & 1) && b.squares[make_sq(5,0)]==make_piece(WHITE,NONE)
                    && b.squares[make_sq(6,0)]==make_piece(WHITE,NONE)
                    && b.squares[make_sq(7,0)]==make_piece(WHITE,ROOK)
                    && !b.is_square_attacked(make_sq(4,0), them)
                    && !b.is_square_attacked(make_sq(5,0), them)
                    && !b.is_square_attacked(make_sq(6,0), them))
                    out.push_back(make_move(from, make_sq(6,0), 0, FLAG_CASTLE));
                if ((b.castling & 2) && b.squares[make_sq(3,0)]==make_piece(WHITE,NONE)
                    && b.squares[make_sq(2,0)]==make_piece(WHITE,NONE)
                    && b.squares[make_sq(1,0)]==make_piece(WHITE,NONE)
                    && b.squares[make_sq(0,0)]==make_piece(WHITE,ROOK)
                    && !b.is_square_attacked(make_sq(4,0), them)
                    && !b.is_square_attacked(make_sq(3,0), them)
                    && !b.is_square_attacked(make_sq(2,0), them))
                    out.push_back(make_move(from, make_sq(2,0), 0, FLAG_CASTLE));
            } else {
                if ((b.castling & 4) && b.squares[make_sq(5,7)]==make_piece(BLACK,NONE)
                    && b.squares[make_sq(6,7)]==make_piece(BLACK,NONE)
                    && b.squares[make_sq(7,7)]==make_piece(BLACK,ROOK)
                    && !b.is_square_attacked(make_sq(4,7), them)
                    && !b.is_square_attacked(make_sq(5,7), them)
                    && !b.is_square_attacked(make_sq(6,7), them))
                    out.push_back(make_move(from, make_sq(6,7), 0, FLAG_CASTLE));
                if ((b.castling & 8) && b.squares[make_sq(3,7)]==make_piece(BLACK,NONE)
                    && b.squares[make_sq(2,7)]==make_piece(BLACK,NONE)
                    && b.squares[make_sq(1,7)]==make_piece(BLACK,NONE)
                    && b.squares[make_sq(0,7)]==make_piece(BLACK,ROOK)
                    && !b.is_square_attacked(make_sq(4,7), them)
                    && !b.is_square_attacked(make_sq(3,7), them)
                    && !b.is_square_attacked(make_sq(2,7), them))
                    out.push_back(make_move(from, make_sq(2,7), 0, FLAG_CASTLE));
            }
            continue;
        }
        // sliding: bishop/rook/queen
        static const int di[4][2] = {{1,1},{1,-1},{-1,1},{-1,-1}};
        static const int li[4][2] = {{1,0},{-1,0},{0,1},{0,-1}};
        auto add_slide = [&](const int (*dirs)[2], int n) {
            for (int i=0;i<n;i++){
                int nf=ff+dirs[i][0], nr=fr+dirs[i][1];
                while(nf>=0&&nf<8&&nr>=0&&nr<8){
                    Square to=make_sq(nf,nr);
                    Piece tgt=b.squares[to];
                    if (piece_type(tgt)==NONE) out.push_back(make_move(from,to,0,FLAG_NORMAL));
                    else {
                        if (piece_color(tgt)==them) out.push_back(make_move(from,to,0,FLAG_CAPTURE));
                        break;
                    }
                    nf+=dirs[i][0]; nr+=dirs[i][1];
                }
            }
        };
        if (pt==BISHOP) add_slide(di,4);
        else if (pt==ROOK) add_slide(li,4);
        else if (pt==QUEEN) { add_slide(di,4); add_slide(li,4); }
    }
    return out;
}

std::vector<Move> naive_legal(const Board& b) {
    std::vector<Move> ps = naive_pseudo(b);
    std::vector<Move> out;
    for (Move m : ps) if (b.is_legal(m)) out.push_back(m);
    return out;
}

uint64_t perft_naive(Board& b, int depth) {
    if (depth == 0) return 1;
    auto mv = naive_legal(b);
    if (depth == 1) return mv.size();
    uint64_t n = 0;
    for (Move m : mv) { Undo u; b.make_move(m, u); n += perft_naive(b, depth-1); b.unmake_move(m, u); }
    return n;
}

} // namespace

std::string mvstr(Move m) {
    std::string s = square_to_string(move_from(m)) + square_to_string(move_to(m));
    if (move_promo(m)) s += std::string(1, "nbrq"[move_promo(m)-1]);
    return s;
}

int main() {
    struct Case { std::string name; std::string fen; std::vector<uint64_t> exp; };
    std::vector<Case> cases = {
        {"kiwipete", "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1", {48,2039,97862}},
        {"kiwipete_b", "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R b KQkq - 0 1", {48,2039,97862}},
        {"position4", "r3k2r/Pppp1ppp/1b3nbN/nP6/BBP1P3/q4N2/Pp1P2PP/R2Q1RK1 w kq - 0 1", {6,264,9467}},
        {"position5", "rnbq1k1r/pp1Pbppp/2p5/8/2B5/8/PPP1NnPP/RNBQK2R w KQ - 1 8", {44,1486,62379}},
    };
    for (auto& c : cases) {
        std::cout << "=== " << c.name << " ===" << std::endl;
        for (int d = 1; d <= (int)c.exp.size(); d++) {
            Board b1(c.fen), b2(c.fen);
            uint64_t fast = hy3::perft(b1, d);
            uint64_t nv = perft_naive(b2, d);
            std::cout << "  depth " << d << ": fast=" << fast << " naive=" << nv
                      << " expected=" << c.exp[d-1]
                      << (fast==c.exp[d-1]?" fastOK":" fastFAIL")
                      << (nv==c.exp[d-1]?" naiveOK":" naiveFAIL") << std::endl;
        }
        // Mostrar diferencia de movimientos legales en la raiz
        Board b(c.fen);
        auto fast = b.legal_moves();
        auto nv = naive_legal(b);
        std::set<std::string> sf, sn;
        for (auto m:fast) sf.insert(mvstr(m));
        for (auto m:nv) sn.insert(mvstr(m));
        std::cout << "  root fast=" << fast.size() << " naive=" << nv.size() << std::endl;
        for (auto s: sf) if (!sn.count(s)) std::cout << "    FAST-only: " << s << std::endl;
        for (auto s: sn) if (!sf.count(s)) std::cout << "    NAIVE-only: " << s << std::endl;
    }
    return 0;
}
