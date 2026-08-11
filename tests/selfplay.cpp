// tests/selfplay.cpp - Verifica una partida completa sin errores
#include "../src/search.h"
#include <iostream>
#include <vector>
#include <cassert>
using namespace hy3;

int main() {
    std::vector<std::string> moves;
    // Autoplay desde startpos, profundidad baja y tiempo corto por movimiento
    std::string result = self_play(Board(), 2, 50, &moves);
    std::cout << "Resultado: " << result << std::endl;
    std::cout << "Num movimientos: " << moves.size() << std::endl;
    // Verificar que todos los movimientos son legales aplicándolos sobre un tablero nuevo
    Board b;
    int i = 0;
    for (auto& u : moves) {
        Move m = 0;
        for (Move cand : b.legal_moves())
            if (square_to_string(move_from(cand)) + square_to_string(move_to(cand)) +
                (move_promo(cand) ? std::string(1,"nbrq"[move_promo(cand)-1]) : std::string()) == u)
                { m = cand; break; }
        assert(("movimiento illegal generado: " + u, m != 0));
        Undo un; b.make_move(m, un);
        i++;
    }
    std::cout << "Todos los movimientos son legales: OK (" << i << ")" << std::endl;
    return 0;
}
