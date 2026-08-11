// tests/refcheck.cpp - Enroques en position5
#include "../src/board.h"
#include <iostream>
using namespace hy3;

int main() {
    Board b("rnbq1k1r/pp1Pbppp/2p5/8/2B5/8/PPP1NnPP/RNBQK2R w KQ - 1 8");
    auto mv = b.legal_moves();
    std::cout << "Total: " << mv.size() << std::endl;
    for (auto m : mv)
        if (move_flag(m) == FLAG_CASTLE)
            std::cout << "ENROQUE: " << square_to_string(move_from(m)) << square_to_string(move_to(m)) << std::endl;
    // Verificar si d1 esta atacado por el caballo negro en f2
    std::cout << "d1 atacado por BLACK: " << b.is_square_attacked(make_sq(3,0), BLACK) << std::endl;
    return 0;
}