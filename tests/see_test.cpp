#include "../src/board.h"
#include "../src/movegen.h"
#include <cstdio>

using namespace hy3;

static int fails = 0;
static void check(const char* name, int got, int exp) {
    if (got == exp) printf("OK   %s got=%d exp=%d\n", name, got, exp);
    else { printf("FAIL %s got=%d exp=%d\n", name, got, exp); fails++; }
}

int main() {
    // 1) Rook a1 captures free pawn a7; no recapture -> +100.
    Board c1; c1.set_from_fen("R6k/p7/8/8/8/8/8/7K w - - 0 1");
    check("Rxa7 free pawn (100)", c1.see(make_sq(0,6), WHITE), 100);

    // 2) Rook a1 captures free black rook d1; no recapture -> +500.
    Board c2; c2.set_from_fen("8/8/8/8/8/8/8/R2r3K w - - 0 1");
    check("Rxd1 free rook (500)", c2.see(make_sq(3,0), WHITE), 500);

    // 3) White rook a1, black rook a8 defended by black rook h8:
    //    white RxR (+500), black RxR (-500) -> net 0.
    Board c3; c3.set_from_fen("r6r/8/8/8/8/8/8/R6K w - - 0 1");
    check("Rxa8 defended rook-pair (0)", c3.see(make_sq(0,7), WHITE), 0);

    // 4) Queen e4 takes pawn d5 defended by pawn c6: Qxd5 (+100), pxd5 (-900) -> -800.
    Board c4; c4.set_from_fen("8/8/2p5/3p4/4Q3/8/8/4K2k w - - 0 1");
    check("Qxd5 defended pawn (-800)", c4.see(make_sq(3,4), WHITE), -800);

    // 5) Knight takes undefended queen: +900.
    Board c5; c5.set_from_fen("3q4/8/2N5/8/8/8/8/7K w - - 0 1");
    check("Nxd8 free queen (900)", c5.see(make_sq(3,7), WHITE), 900);

    // 6) Black rook a2 takes white rook a1, white cannot recapture: +500.
    Board c6; c6.set_from_fen("6k1/8/8/8/8/8/r7/R6K b - - 0 1");
    check("bxa1 free rook (500)", c6.see(make_sq(0,0), BLACK), 500);

    // 7) No attacker: square empty, white to move, nothing attacks -> 0.
    Board c7; c7.set_from_fen("8/8/8/8/8/8/8/7K w - - 0 1");
    check("empty square (0)", c7.see(make_sq(4,4), WHITE), 0);

    if (fails == 0) printf("ALL SEE TESTS PASSED\n");
    else printf("%d SEE TESTS FAILED\n", fails);
    return fails ? 1 : 0;
}
