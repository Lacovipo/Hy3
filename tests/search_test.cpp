// tests/search_test.cpp - Verifica que search no corrompe el tablero
#include "../src/search.h"
#include <algorithm>
#include <iostream>
using namespace hy3;

static bool legal_pv(Board b, const std::vector<Move>& pv) {
    for (Move m : pv) {
        auto legal = b.legal_moves();
        if (std::find(legal.begin(), legal.end(), m) == legal.end()) return false;
        Undo u;
        b.make_move(m, u);
    }
    return true;
}

int main() {
    int fails = 0;
    Board orig;
    Board b = orig;  // copia explícita (como en self_play)
    std::string start = b.to_fen();
    long mk0, um0; debug_counts(mk0, um0);
    SearchResult r = search(b, SearchLimits{2, 50});
    long mk1, um1; debug_counts(mk1, um1);
    std::cout << "start fen: " << start << std::endl;
    std::cout << "after fen: " << b.to_fen() << std::endl;
    std::cout << "make=" << (mk1-mk0) << " unmake=" << (um1-um0)
              << " balanced=" << ((mk1-mk0)==(um1-um0)?"YES":"NO") << std::endl;
    std::cout << "restored=" << (b.to_fen()==start ? "YES":"NO") << std::endl;
    std::cout << "best=" << (r.best ? square_to_string(move_from(r.best))+square_to_string(move_to(r.best)) : std::string("0")) << std::endl;
    if (b.to_fen() != start || r.best == 0) fails++;

    // Regresion: esta posicion provocaba un acceso fuera de rango por superar
    // MAX_PLY en cadenas de jaques y ademas publicaba una PV mezclada.
    Board tactical("rn2k2r/ppp2ppp/3q1n2/3p1p2/3P4/4PN2/PPP2PPP/RN1QK2R w KQkq - 0 8");
    bool pv_ok = true;
    SearchLimits lim;
    lim.max_depth = 6;
    lim.time_ms = 2000;
    lim.on_info = [&](int, Score, uint64_t, int64_t, const std::vector<Move>& pv) {
        if (!legal_pv(tactical, pv)) pv_ok = false;
    };
    SearchResult tactical_result = search(tactical, lim);
    std::cout << "tactical completed=" << (tactical_result.best != 0 ? "YES":"NO") << std::endl;
    std::cout << "tactical pv legal=" << (pv_ok ? "YES":"NO") << std::endl;
    if (tactical_result.best == 0 || !pv_ok) fails++;
    return fails == 0 ? 0 : 1;
}
