// search.h - Búsqueda del motor hy3
#ifndef HY3_SEARCH_H
#define HY3_SEARCH_H

#include "board.h"
#include "eval.h"
#include <vector>
#include <cstdint>
#include <string>
#include <functional>

namespace hy3 {

const Score MATE = 30000;

// Callback de información (UCI "info depth ... pv ...") por iteración.
using InfoCallback = std::function<void(int depth, Score score, uint64_t nodes, int64_t time_ms, const std::vector<Move>& pv)>;

struct SearchLimits {
    int max_depth = 6;
    int64_t time_ms = 1000; // límite de tiempo (0 = sin límite)
    int64_t max_time_ms = 0; // techo duro opcional (0 = usar time_ms)
    bool ponder = false;     // búsqueda en tiempo del rival (go ponder)
    InfoCallback on_info;   // opcional: se llama tras cada iteración
};

struct SearchResult {
    Move best = 0;
    Move ponder = 0;        // jugada esperada del rival (2º movimiento de la PV)
    Score score = 0;
    int depth = 0;
    uint64_t nodes = 0;
};

bool insufficient_material(const Board& b);

// Limpia la tabla de transposición (usada en "ucinewgame").
void tt_clear();

// Redimensiona la tabla de transposición a 'mb' megabytes (opción UCI "Hash").
void tt_resize(int mb);

// Establece el historial de claves Zobrist de la partida real (para detección
// de triple repetición durante la búsqueda). 'keys' contiene las posiciones ya
// jugadas (incluida la posición raíz actual como última entrada opcional).
void set_game_history(const std::vector<uint64_t>& keys);

// Búsqueda principal con profundización iterativa y gestión de tiempo.
SearchResult search(Board& b, const SearchLimits& lim);

// Señal de parada externa (para el comando "stop" de UCI).
void signal_stop();
void clear_stop();

// Ponder: mientras está activo, la búsqueda ignora el límite de tiempo.
// ponderhit() lo desactiva y arranca el reloj real de la jugada.
void set_pondering(bool on);
void ponder_hit();

// Depuración: activa la traza del árbol de búsqueda (entrada/salida de nodo).
void set_tree_debug(bool on, int max_depth_print);

// Jugador automático: juega una partida completa (para pruebas de autoplay).
// Devuelve el PGN y el resultado. 'moves' opcionalmente recibe la lista UCI.
std::string self_play(Board b, int max_depth, int time_ms, std::vector<std::string>* moves = nullptr);

} // namespace hy3

#endif // HY3_SEARCH_H
