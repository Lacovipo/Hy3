// tests/debug_moves.cpp - Imprime movimientos pseudolegales de startpos
#include "../src/board.h"
#include "../src/movegen.h"
#include <iostream>

using namespace hy3;

int main() {
    Board b("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");
    std::cout << "side=" << b.side << " (0=WHITE,1=BLACK)" << std::endl;
    std::cout << "piece at a8 color=" << piece_color(b.squares[make_sq(0,7)])
              << " type=" << piece_type(b.squares[make_sq(0,7)]) << std::endl;
    std::cout << "piece at a1 color=" << piece_color(b.squares[make_sq(0,0)])
              << " type=" << piece_type(b.squares[make_sq(0,0)]) << std::endl;
    std::cout << "to_fen: " << b.to_fen() << std::endl;
    auto moves = b.pseudo_moves();
    std::cout << "pseudo count = " << moves.size() << std::endl;
    for (auto m : moves) {
        std::cout << square_to_string(move_from(m)) << square_to_string(move_to(m))
                  << " flag=" << move_flag(m) << " promo=" << move_promo(m) << std::endl;
    }
    return 0;
}