// tests/movelist.cpp - Vuelca movimientos legales para un FEN dado (unidad de prueba vs python-chess)
#include "../src/board.h"
#include <iostream>
#include <string>
#include <vector>
using namespace hy3;

std::string mvstr(Move m) {
    std::string s = square_to_string(move_from(m)) + square_to_string(move_to(m));
    if (move_promo(m)) s += "nbrq"[move_promo(m)-1];
    return s;
}

int main(int argc, char** argv) {
    if (argc < 2) { std::cerr << "uso: movelist <fen> [fen]\n"; return 1; }
    Board b(argv[1]);
    auto mv = b.legal_moves();
    if (argc >= 3 && std::string(argv[2]) == "fen") {
        for (auto m : mv) {
            Board c = b;
            Undo u;
            c.make_move(m, u);
            std::cout << mvstr(m) << " " << c.to_fen() << "\n";
        }
    } else {
        std::cout << mv.size() << "\n";
        for (auto m : mv) std::cout << mvstr(m) << "\n";
    }
    return 0;
}
