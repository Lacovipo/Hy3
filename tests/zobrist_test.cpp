// zobrist_test.cpp - Verifica que la clave Zobrist incremental (zkey) coincide
// con hash() recalculado desde cero en cada nodo de un perft.
#include "board.h"
#include "movegen.h"
#include <iostream>
using namespace hy3;

static long mismatches = 0;

uint64_t perft(Board& b, int d) {
    if (b.zkey != b.hash()) {
        mismatches++;
    }
    if (d == 0) return 1;
    auto ms = b.legal_moves();
    uint64_t n = 0;
    for (Move m : ms) {
        Undo u; b.make_move(m, u);
        n += perft(b, d - 1);
        b.unmake_move(m, u);
    }
    return n;
}

int main() {
    const char* fens[] = {
        "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1",
        "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1",
        "8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - - 0 1",
    };
    int depths[] = { 4, 3, 4 };
    for (int i = 0; i < 3; i++) {
        Board b(fens[i]);
        perft(b, depths[i]);
    }
    if (mismatches == 0) std::cout << "OK  zkey incremental == hash() en todos los nodos\n";
    else std::cout << "FAIL  " << mismatches << " discrepancias de hash\n";
    return mismatches == 0 ? 0 : 1;
}
