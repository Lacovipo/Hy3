// board.h - Representación del tablero y operaciones básicas
#ifndef HY3_BOARD_H
#define HY3_BOARD_H

#include "types.h"
#include <string>
#include <vector>

namespace hy3 {

struct Undo {
    Piece captured;       // pieza capturada (NONE si no)
    Square ep_square;     // ep anterior
    int castling;         // derechos anteriores
    int halfmove;         // reloj de mediacaptura anterior
    int move_flag;        // flag del move aplicado
    Piece promo_piece;    // pieza promocionada (para deshacer)
    uint64_t zkey;        // clave Zobrist anterior (restauración O(1) en unmake)
};

struct Board {
    Piece squares[N_SQ];
    Color side;
    Square ep_square;     // NO_SQ si no hay
    int castling;         // bits: 1=WK,2=WQ,4=BK,8=BQ
    int halfmove;
    int fullmove;
    Square king_sq[2];    // posición de cada rey
    uint64_t zkey;        // clave Zobrist incremental (actualizada en make/unmake)

    Board();
    explicit Board(const std::string& fen);

    void set_from_fen(const std::string& fen);
    std::string to_fen() const;

    // Aplica un movimiento (asume legal o pseudolegal; actualiza estado)
    void make_move(Move m, Undo& u);
    void unmake_move(Move m, const Undo& u);

    // ¿Está sq atacada por color 'by'?
    bool is_square_attacked(Square sq, Color by) const;
    bool in_check(Color c) const { return is_square_attacked(king_sq[c], Color(c ^ 1)); }
    bool in_check() const { return in_check(side); }

    // Genera todos los movimientos pseudolegales (sin filtrar jaque)
    std::vector<Move> pseudo_moves() const;
    // Genera movimientos legales (filtra jaque al propio rey)
    std::vector<Move> legal_moves() const;
    // Versión sin asignaciones dinámicas: rellena un MoveList del stack.
    void legal_moves(MoveList& out) const;
    int legal_move_count() const;
    bool is_legal(Move m) const;

    // Clave Zobrist para detección de repetición / transposición.
    // hash() recalcula desde cero (referencia/depuración); zkey se mantiene
    // incrementalmente durante make_move/unmake_move y es el camino caliente.
    uint64_t hash() const;
    // Recalcula zkey desde cero (usado tras set_from_fen).
    void recompute_zkey();
    Piece piece_at(Square s) const { return squares[s]; }

    // Static Exchange Evaluation: valor del intercambio si el lado 'us' captura
    // en 'to' (siendo la pieza capturada la que hay en 'to'). Devuelve el
    // balance (en centipeones) de jugar el intercambio completo.
    // gain = (valor de lo que capturamos) - (lo que perderíamos en la réplica).
    int see(Square to, Color us) const;

    // SEE de una JUGADA concreta: a diferencia de see(to, us), que siempre
    // asume que captura el atacante más barato, see_move() arranca el
    // intercambio con la pieza que realmente se mueve. Así Dxd5 y exd5 dejan
    // de recibir la misma puntuación. Maneja además capturas al paso y
    // promociones.
    int see_move(Move m) const;

    // ¿La posición es válida para buscar? (exactamente un rey por bando)
    bool is_valid() const { return king_sq[WHITE] != NO_SQ && king_sq[BLACK] != NO_SQ; }
};

// Valida un FEN sintácticamente (campos, 8 filas que suman 8 casillas, un rey
// por bando, turno válido y casilla al paso coherente). Devuelve true si es
// válido; si no, escribe en 'err' la razón. No modifica el tablero. (Fix #8)
bool validate_fen(const std::string& fen, std::string& err);

// (Depuración) contadores de make/unmake
void debug_counts(long& mk, long& um);

// Componentes Zobrist expuestos para la actualización incremental del
// null-move en la búsqueda (evita recalcular la clave entera).
uint64_t zobrist_side();
uint64_t zobrist_ep(Square s);

// Utilidades de coordenadas
inline int sq_file(Square s) { return s & 7; }
inline int sq_rank(Square s) { return s >> 3; }
inline Square make_sq(int f, int r) { return r * 8 + f; }
std::string square_to_string(Square s);
Square string_to_square(const std::string& s);

} // namespace hy3

#endif // HY3_BOARD_H