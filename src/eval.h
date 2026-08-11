// eval.h - Evaluación estática del motor hy3
#ifndef HY3_EVAL_H
#define HY3_EVAL_H

#include "board.h"
#include "movegen.h"

namespace hy3 {

// Valores de material (en centipeones)
extern const int PIECE_VALUE[6]; // PAWN, KNIGHT, BISHOP, ROOK, QUEEN, KING

// Evalúa la posición desde la perspectiva del lado que mueve (negamax).
// Compuesta por: material + piece-square tables + movilidad.
Score evaluate(const Board& b);

} // namespace hy3

#endif // HY3_EVAL_H
