#include "../src/board.h"
#include "../src/perft.h"
#include <iostream>
#include <string>
#include <vector>
#include <map>
using namespace hy3;

std::string mvstr(Move m) {
    std::string s = square_to_string(move_from(m)) + square_to_string(move_to(m));
    if (move_promo(m)) s += "nbrq"[move_promo(m)-1];
    return s;
}

int main() {
    std::string fen = "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1";
    Board b(fen);
    auto moves = b.legal_moves();
    std::cout << "Total root moves: " << moves.size() << std::endl;
    std::map<std::string,uint64_t> counts;
    uint64_t total = 0;
    for (auto m : moves) {
        Board bb(fen);
        Undo u;
        bb.make_move(m, u);
        uint64_t c = perft(bb, 1); // black replies count
        counts[mvstr(m)] = c;
        total += c;
    }
    for (auto& kv : counts)
        std::cout << "  " << kv.first << " : " << kv.second << std::endl;
    std::cout << "SUM = " << total << " (expected 2039)" << std::endl;
    return 0;
}
