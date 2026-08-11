// tests/time_test.cpp - Mide si search respeta el límite de tiempo
#include "../src/board.h"
#include "../src/search.h"
#include "../src/movegen.h"
#include <iostream>
#include <chrono>
#include <vector>
#include <string>
using namespace hy3;

static Move parse(Board& b, const std::string& s) {
    for (Move m : b.legal_moves())
        if (square_to_string(move_from(m)) + square_to_string(move_to(m)) +
            (move_promo(m) ? std::string(1, "nbrq"[move_promo(m)-1]) : std::string()) == s)
            return m;
    return 0;
}

int main() {
    Board b;
    for (auto& u : std::vector<std::string>{"e2e4","a7a6","d2d4"}) {
        Move m = parse(b, u);
        if (!m) { std::cout << "bad move " << u << "\n"; return 1; }
        Undo un; b.make_move(m, un);
    }
    std::cout << "pos: " << b.to_fen() << "\n";
    auto t0 = std::chrono::steady_clock::now();
    SearchResult r = search(b, SearchLimits{64, 2700});
    auto t1 = std::chrono::steady_clock::now();
    double ms = std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0).count();
    std::string best = r.best ? square_to_string(move_from(r.best)) + square_to_string(move_to(r.best)) : std::string("0");
    std::cout << "elapsed_ms=" << ms << " best=" << best
              << " depth=" << r.depth << " nodes=" << r.nodes << "\n";
    return 0;
}
