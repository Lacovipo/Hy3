// movegen.h - Generador de movimientos pseudolegales
#ifndef HY3_MOVEGEN_H
#define HY3_MOVEGEN_H

#include "board.h"
#include <vector>

namespace hy3 {

// Genera movimientos pseudolegales para el lado en turno (no filtra jaque).
// Versión sin asignaciones: rellena un MoveList basado en el stack.
void generate_pseudo_moves(const Board& b, MoveList& out);
// Compatibilidad: variante que devuelve un std::vector (usada por perft/tests).
std::vector<Move> generate_pseudo_moves(const Board& b);

// Cuenta movimientos pseudolegales del lado en turno sin construir un vector.
int count_pseudo_moves(const Board& b);
// Variante que cuenta para un color arbitrario (sin copiar el tablero).
int count_pseudo_moves(const Board& b, Color us);

// Genera SOLO capturas y promociones (pseudolegales) para el lado en turno.
// Usado por la búsqueda de quiescencia: hasta v1.5 la QS generaba todos los
// movimientos legales y descartaba ~85 % de ellos.
void generate_captures(const Board& b, MoveList& out);

// Movilidad por pieza para la evaluación: devuelve el número de casillas
// alcanzables por las piezas de 'us', excluyendo peones y rey (movilidad
// "de piezas", que es la señal posicional útil) y sin contar las casillas
// atacadas por peones enemigos.
int piece_mobility(const Board& b, Color us);

} // namespace hy3

#endif // HY3_MOVEGEN_H