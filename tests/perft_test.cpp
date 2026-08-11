#include "board.h"
#include "movegen.h"
#include <iostream>
using namespace hy3;
uint64_t perft(Board& b, int d) {
    if (d == 0) return 1;
    auto ms = b.legal_moves();
    if (d == 1) return ms.size();
    uint64_t n = 0;
    for (Move m : ms) { Undo u; b.make_move(m,u); n += perft(b,d-1); b.unmake_move(m,u); }
    return n;
}
int main() {
    struct { const char* fen; int d; uint64_t exp; } T[] = {
        {"rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1", 4, 197281},
        {"r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1", 3, 97862},
        {"8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - - 0 1", 4, 43238},
    };
    bool ok = true;
    for (auto& t : T) {
        Board b(t.fen);
        uint64_t r = perft(b, t.d);
        std::cout << (r==t.exp ? "OK  " : "FAIL") << " d" << t.d << " got=" << r << " exp=" << t.exp << "\n";
        if (r != t.exp) ok = false;
    }
    return ok ? 0 : 1;
}
