// perft.cpp - Implementación de perft
#include "perft.h"

namespace hy3 {

uint64_t perft(Board& b, int depth) {
    if (depth == 0) return 1;
    std::vector<Move> moves = b.legal_moves();
    if (depth == 1) return moves.size();
    uint64_t nodes = 0;
    for (Move m : moves) {
        Undo u;
        b.make_move(m, u);
        nodes += perft(b, depth - 1);
        b.unmake_move(m, u);
    }
    return nodes;
}

} // namespace hy3