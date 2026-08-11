// tests/legality.cpp - Tests dirigidos de jaque, clavada y enroque
#include "../src/board.h"
#include <algorithm>
#include <iostream>
using namespace hy3;

static int fails = 0;
void check(const std::string& name, bool got, bool exp) {
    std::cout << (got==exp?"OK  ":"FAIL") << " " << name
              << " got=" << (got?"T":"F") << " exp=" << (exp?"T":"F") << std::endl;
    if (got!=exp) fails++;
}

bool has_legal_move(const Board& b, Move move) {
    auto moves = b.legal_moves();
    return std::find(moves.begin(), moves.end(), move) != moves.end();
}

int main() {
    // 1) Clavada vertical: Rey blanco e1, torre blanca e2, torre negra e8 (rey negro h8)
    {
        Board d("4r2k/8/8/8/8/8/4R3/4K3 w - - 0 1");
        Move onFile = make_move(make_sq(4,1), make_sq(4,3), 0, 0); // e2e4 (sigue en columna e) -> legal
        Move offFile = make_move(make_sq(4,1), make_sq(3,1), 0, 0); // e2d2 (fuera de columna) -> ilegal
        check("pin-vertical on-file legal", d.is_legal(onFile), true);
        check("pin-vertical off-file illegal", d.is_legal(offFile), false);
    }
    // 2) Clavada diagonal: Rey blanco e1, alfil blanca f2, dama negra h4 (e1-f2-g3-h4)
    {
        Board b("7k/8/8/8/7q/8/5B2/4K3 w - - 0 1");
        Move along = make_move(make_sq(5,1), make_sq(6,2), 0, 0); // f2g3 sigue en diagonal -> legal
        Move off = make_move(make_sq(5,1), make_sq(5,2), 0, 0);   // f2f3 fuera diagonal -> ilegal
        check("pin-diag along legal", b.is_legal(along), true);
        check("pin-diag off illegal", b.is_legal(off), false);
    }
    // 3) Jaque: rey blanco e1, dama negra e8 (columna). Mover rey fuera -> legal; en columna -> ilegal
    {
        Board b("4q3k/8/8/8/8/8/8/4K3 w - - 0 1");
        Move e1d1 = make_move(make_sq(4,0), make_sq(3,0), 0, 0); // fuera de columna e -> legal
        Move e1e2 = make_move(make_sq(4,0), make_sq(4,1), 0, 0); // sigue en e -> ilegal
        check("check-king off-file legal", b.is_legal(e1d1), true);
        check("check-king on-file illegal", b.is_legal(e1e2), false);
    }
    // 4) Enroque con camino bloqueado por jaque: dama negra f8 ataca f1
    {
        Board b("5q1k/8/8/8/8/8/8/4K2R w K - 0 1");
        Move oo = make_move(make_sq(4,0), make_sq(6,0), 0, FLAG_CASTLE);
        check("castle through check illegal", has_legal_move(b, oo), false);
        // sin dama atacante: debe ser legal
        Board b2("7k/8/8/8/8/8/8/4K2R w K - 0 1");
        check("castle path safe legal", has_legal_move(b2, oo), true);
    }
    // 5) is_square_attacked basicos
    {
        Board b("8/8/8/8/8/8/4P3/8 w - - 0 1");
        check("pawn attacks d3", b.is_square_attacked(make_sq(3,2), WHITE), true);
        check("pawn attacks f3", b.is_square_attacked(make_sq(5,2), WHITE), true);
        check("pawn not attack e3", b.is_square_attacked(make_sq(4,2), WHITE), false);
    }
    // 6) Ataque de torre bloqueado
    {
        Board b("8/8/8/8/8/8/3r4/3R4 w - - 0 1");
        check("rook attacks adjacent", b.is_square_attacked(make_sq(3,0), BLACK), true);
        Board b2("3r4/8/8/3R4/8/8/8/8 w - - 0 1");
        check("rook blocked not attack", b2.is_square_attacked(make_sq(3,3), BLACK), false);
    }
    // 7) El rey ya esta en jaque por una torre en e8 y no puede enrocar.
    {
        Board b("4r2k/8/8/8/8/8/8/4K2R w K - 0 1");
        Move oo = make_move(make_sq(4,0), make_sq(6,0), 0, FLAG_CASTLE);
        check("castle while in check illegal", has_legal_move(b, oo), false);
    }
    // 8) Los derechos de enroque se pierden al mover la torre y se restauran al deshacer.
    {
        Board b("4k3/8/8/8/8/8/8/R3K2R w KQ - 0 1");
        Move rh2 = make_move(make_sq(7,0), make_sq(7,1));
        Undo u;
        b.make_move(rh2, u);
        check("rook move clears kingside castling", (b.castling & 1) != 0, false);
        check("rook move preserves queenside castling", (b.castling & 2) != 0, true);
        b.unmake_move(rh2, u);
        check("unmake restores kingside castling", (b.castling & 1) != 0, true);
    }
    // 9) Un FEN no puede conceder enroque sin rey y torre en sus casillas iniciales.
    {
        Board noRook("4k3/8/8/8/8/8/8/4K3 w KQ - 0 1");
        check("fen clears castling without rooks", noRook.castling != 0, false);
        Board displacedKing("4k3/8/8/8/8/8/8/R4K1R w KQ - 0 1");
        check("fen clears castling with displaced king", displacedKing.castling != 0, false);
    }
    // 10) El doble avance solo crea ep_square si hay un peon rival adyacente.
    {
        Board withCapturer("4k3/8/8/8/3p4/8/4P3/4K3 w - - 0 1");
        Move e4 = make_move(make_sq(4,1), make_sq(4,3));
        Undo u;
        withCapturer.make_move(e4, u);
        check("double push sets capturable ep square", withCapturer.ep_square == make_sq(4,2), true);

        Board withoutCapturer("4k3/8/8/8/8/8/4P3/4K3 w - - 0 1");
        withoutCapturer.make_move(e4, u);
        check("double push omits uncapturable ep square", withoutCapturer.ep_square == NO_SQ, true);
    }
    std::cout << (fails==0 ? "ALL LEGALITY TESTS PASSED" : "SOME FAILED") << std::endl;
    return fails==0?0:1;
}
