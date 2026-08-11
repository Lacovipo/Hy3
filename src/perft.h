// perft.h - Conteo de nodos para verificación del generador de movimientos
#ifndef HY3_PERFT_H
#define HY3_PERFT_H

#include "board.h"
#include <cstdint>

namespace hy3 {

// Cuenta nodos a profundidad 'depth' usando movimientos legales
uint64_t perft(Board& b, int depth);

} // namespace hy3

#endif // HY3_PERFT_H