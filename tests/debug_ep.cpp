// tests/debug_ep.cpp - Verifica captura al paso make/unmake
#include "../src/board.h"
#include <iostream>
using namespace hy3;

int main() {
    // Posicion: blanco peon e5, negro peon d5 acaba de jugar d7-d5, ep en d6
    // Blanco juega exd6 ep
    Board b("4k3/ppp1pppp/8/3pP3/8/8/PPPP1PPP/4K3 w - d6 0 1");
    std::cout << "Antes: " << b.to_fen() << std::endl;
    Move ep = make_move(make_sq(4,4), make_sq(3,5), 0, FLAG_EP); // e5xd6
    Undo u;
    b.make_move(ep, u);
    std::cout << "Tras exd6: " << b.to_fen() << std::endl;
    std::cout << "Peon en d6? " << (piece_type(b.squares[make_sq(3,5)])==PAWN) << std::endl;
    std::cout << "Peon negro en d5 eliminado? " << (piece_type(b.squares[make_sq(3,4)])==NONE) << std::endl;
    b.unmake_move(ep, u);
    std::cout << "Tras unmake: " << b.to_fen() << std::endl;
    std::cout << "Restaurado igual a antes? " << (b.to_fen()=="4k3/ppp1pppp/8/3pP3/8/8/PPPP1PPP/4K3 w - d6 0 1") << std::endl;
    return 0;
}