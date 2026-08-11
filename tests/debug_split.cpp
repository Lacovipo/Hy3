// tests/debug_split.cpp - Prueba acumulacion en unmake
#include "../src/board.h"
#include "../src/perft.h"
#include <iostream>
using namespace hy3;

int main() {
    Board b("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");
    Undo u;
    // Hacer Nc3 y deshacer
    b.make_move(make_move(make_sq(1,0), make_sq(2,2), 0, 0), u);
    b.unmake_move(make_move(make_sq(1,0), make_sq(2,2), 0, 0), u);
    std::cout << "Tras Nc3+unmake: " << b.to_fen() << std::endl;
    // Ahora Na3 y perft(b,2)
    b.make_move(make_move(make_sq(1,0), make_sq(0,2), 0, 0), u);
    uint64_t n = perft(b, 2);
    b.unmake_move(make_move(make_sq(1,0), make_sq(0,2), 0, 0), u);
    std::cout << "Na3 perft(b,2) tras Nc3+unmake = " << n << " (esperado 400)" << std::endl;
    return 0;
}