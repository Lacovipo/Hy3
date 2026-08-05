// types.h - Tipos básicos compartidos del motor hy3
#ifndef HY3_TYPES_H
#define HY3_TYPES_H

#include <cstdint>

namespace hy3 {

// Identidad del motor
constexpr const char* ENGINE_NAME = "Hy3";
constexpr const char* ENGINE_VERSION = "1.6";
constexpr const char* ENGINE_ID = "Hy3 1.6";

// Casillas 0..63 (a1=0, b1=1, ..., h8=63)
using Square = int;
using Piece = int;
using Move = uint32_t;
using Score = int;

enum Color : int { WHITE = 0, BLACK = 1 };
enum PieceType : int {
    PAWN = 0, KNIGHT = 1, BISHOP = 2, ROOK = 3, QUEEN = 4, KING = 5, NONE = 6
};

// Codificación de pieza: color*8 + tipo  (0..11)
constexpr Piece make_piece(Color c, PieceType t) { return c * 8 + t; }
// Casilla vacía = (cualquier color, NONE). Constante con nombre para claridad.
constexpr Piece EMPTY = make_piece(WHITE, NONE);
inline Color piece_color(Piece p) { return Color(p >> 3); }
inline PieceType piece_type(Piece p) { return PieceType(p & 7); }

// Codificación de Move (32 bits):
//  bits 0-5  : from square
//  bits 6-11 : to square
//  bits 12-14: promotion piece type (0=none,1=knight,2=bishop,3=rook,4=queen)
//  bits 15-17: move flags (0 normal,1 capture,2 ep,3 castle)
inline Move make_move(Square from, Square to, int promo = 0, int flag = 0) {
    return (uint32_t(from)) | (uint32_t(to) << 6) | (uint32_t(promo) << 12) | (uint32_t(flag) << 15);
}
inline Square move_from(Move m) { return Square(m & 63); }
inline Square move_to(Move m) { return Square((m >> 6) & 63); }
inline int move_promo(Move m) { return int((m >> 12) & 7); }
inline int move_flag(Move m) { return int((m >> 15) & 7); }

// Buffer estático de movimientos basado en el stack. Evita las asignaciones
// dinámicas de std::vector<Move> en los caminos calientes del generador y la
// búsqueda. El máximo teórico de movimientos legales en una posición de ajedrez
// es 218; 256 da margen de sobra.
struct MoveList {
    Move moves[256];
    int count = 0;
    inline void push_back(Move m) { moves[count++] = m; }
    inline void clear() { count = 0; }
    inline int size() const { return count; }
    inline bool empty() const { return count == 0; }
    inline Move  operator[](int i) const { return moves[i]; }
    inline Move& operator[](int i)       { return moves[i]; }
    inline Move* begin() { return moves; }
    inline Move* end()   { return moves + count; }
    inline const Move* begin() const { return moves; }
    inline const Move* end()   const { return moves + count; }
};

const int FLAG_NORMAL = 0;
const int FLAG_CAPTURE = 1;
const int FLAG_EP = 2;
const int FLAG_CASTLE = 3;

// Constantes de tablero
const int N_SQ = 64;
const Square NO_SQ = -1;

} // namespace hy3

#endif // HY3_TYPES_H