#include "../src/board.h"
#include <iostream>
using namespace hy3;

void show(const std::string& fen, const std::string& label) {
    Board b(fen);
    auto mv = b.pseudo_moves();
    int castles = 0;
    std::cout << label << "  [" << fen << "]\n";
    std::cout << "  pseudo=" << mv.size() << " legal=" << b.legal_moves().size() << "\n";
    for (auto m : mv)
        if (move_flag(m) == FLAG_CASTLE) {
            castles++;
            std::cout << "    CASTLE " << square_to_string(move_from(m)) << square_to_string(move_to(m)) << "\n";
        }
    if (!castles) std::cout << "    (no castle generated)\n";
}

extern int g_dbg;
int main() {
    g_dbg = 1;
    // Simple: black to move, can castle both sides
    show("r3k2r/8/8/8/8/8/8/R3K2R b KQkq - 0 1", "simple black-both");
    // After white makes a quiet move (recursive position)
    show("r3k2r/8/8/8/8/8/8/R3K2R w KQkq - 0 1", "simple white-both");
    // kiwipete black to move
    show("r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R b KQkq - 0 1", "kiwipete black");
    return 0;
}
