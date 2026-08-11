// tests/debug_attack.cpp - Verifica is_square_attacked y is_legal
#include "../src/board.h"
#include <iostream>
using namespace hy3;

int main() {
    // Peón blanco en e2 (4,1) ataca d3(3,2) y f3(5,2)
    Board b3("8/8/8/8/8/8/4P3/8 w - - 0 1");
    std::cout << "d3 attacked by WHITE (true): "
              << b3.is_square_attacked(make_sq(3,2), WHITE) << std::endl;
    std::cout << "f3 attacked by WHITE (true): "
              << b3.is_square_attacked(make_sq(5,2), WHITE) << std::endl;
    std::cout << "d1 attacked by WHITE (false): "
              << b3.is_square_attacked(make_sq(3,0), WHITE) << std::endl;

    // Peón negro en e7 (4,6) ataca d6(3,5) y f6(5,5)
    Board b4("8/4p3/8/8/8/8/8/8 w - - 0 1");
    std::cout << "d6 attacked by BLACK (true): "
              << b4.is_square_attacked(make_sq(3,5), BLACK) << std::endl;
    std::cout << "f6 attacked by BLACK (true): "
              << b4.is_square_attacked(make_sq(5,5), BLACK) << std::endl;

    // Caballo blanco en b1 (1,0) ataca a3(0,2), c3(2,2)
    Board b5("8/8/8/8/8/8/8/N7 w - - 0 1");
    std::cout << "a3 attacked by WHITE (true): "
              << b5.is_square_attacked(make_sq(0,2), WHITE) << std::endl;
    std::cout << "c3 attacked by WHITE (true): "
              << b5.is_square_attacked(make_sq(2,2), WHITE) << std::endl;

    // Alfil blanco en c1 (2,0) ataca a3(0,2), b2(1,1), d2(3,1), e3(4,2)...
    Board b6("8/8/8/8/8/8/8/2B5 w - - 0 1");
    std::cout << "a3 attacked by WHITE (true): "
              << b6.is_square_attacked(make_sq(0,2), WHITE) << std::endl;
    std::cout << "h6 attacked by WHITE (true): "
              << b6.is_square_attacked(make_sq(7,5), WHITE) << std::endl;

    // Rey en jaque: rey blanco e1, dama negra e8 -> e1 atacado por BLACK
    Board b7("4q3/8/8/8/8/8/8/4K3 w - - 0 1");
    std::cout << "e1 attacked by BLACK (true): "
              << b7.is_square_attacked(make_sq(4,0), BLACK) << std::endl;

    // is_legal: rey blanco en e1 en jaque de dama e8. Mover rey a e2 (sigue en columna e) debe ser ILEGAL.
    // Mover rey a d1 (fuera de columna) debe ser LEGAL.
    Board b8("4q3/8/8/8/8/8/8/4K3 w - - 0 1");
    Move m_e2 = make_move(make_sq(4,0), make_sq(4,1), 0, 0);
    Move m_d1 = make_move(make_sq(4,0), make_sq(3,0), 0, 0);
    std::cout << "e1e2 legal (false): " << b8.is_legal(m_e2) << std::endl;
    std::cout << "e1d1 legal (true): " << b8.is_legal(m_d1) << std::endl;
    return 0;
}