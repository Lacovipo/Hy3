// uci.cpp - Interfaz UCI del motor hy3
#include "board.h"
#include "search.h"
#include "movegen.h"
#include <iostream>
#include <string>
#include <sstream>
#include <thread>
#include <vector>
#include <atomic>
#include <chrono>

using namespace hy3;

static Board g_board;
static std::thread g_search_thread;
static std::atomic<bool> g_searching(false);
static Move g_best_move = 0;
static SearchResult g_last_result;
static std::vector<uint64_t> g_history;   // claves Zobrist de las posiciones jugadas

// Convierte un Move a UCI (ej. "e2e4", "e7e8q")
static std::string move_to_uci(Move m) {
    std::string s = square_to_string(move_from(m)) + square_to_string(move_to(m));
    if (move_promo(m)) s += "nbrq"[move_promo(m) - 1];
    return s;
}

static Move uci_to_move(const Board& b, const std::string& s) {
    // Busca el movimiento legal que coincida con la notación UCI
    auto moves = b.legal_moves();
    for (Move m : moves)
        if (move_to_uci(m) == s) return m;
    return 0;
}

// Ejecuta la búsqueda en un hilo sobre una COPIA del tablero (evita la
// condición de carrera con g_board si llega "position" durante la parada).
static void run_search(SearchLimits lim, Board board) {
    g_last_result = search(board, lim);
    g_best_move = g_last_result.best;
    if (g_best_move != 0)
        std::cout << "bestmove " << move_to_uci(g_best_move) << std::endl;
    else
        std::cout << "bestmove 0000" << std::endl;
    g_searching = false;
}

// Aplica una secuencia de movimientos UCI y registra el historial de claves
// Zobrist de cada posición intermedia (para detección de triple repetición).
static void apply_moves(Board& b, const std::string& moves_part) {
    std::istringstream iss(moves_part);
    std::string tok;
    while (iss >> tok) {
        g_history.push_back(b.zkey);   // posición antes de jugar 'tok'
        Move m = uci_to_move(b, tok);
        if (m == 0) break;
        Undo u; b.make_move(m, u);
    }
}

int main() {
    std::cout << std::unitbuf;  // flush inmediato (importante para UCI)
    std::string line;
    while (std::getline(std::cin, line)) {
        std::istringstream iss(line);
        std::string cmd;
        if (!(iss >> cmd)) continue;

        if (cmd == "uci") {
            std::cout << "id name " << ENGINE_ID << "\n";
            std::cout << "id author " << ENGINE_NAME << "\n";
            std::cout << "option name Hash type spin default 64 min 1 max 4096\n";
            std::cout << "uciok\n";
        } else if (cmd == "setoption") {
            // formato: setoption name <Nombre> [value <Valor>]
            std::string tok, name, value;
            while (iss >> tok) {
                if (tok == "name") { std::string w; while (iss >> w && w != "value") { if (!name.empty()) name += " "; name += w; } if (w == "value") { std::string v; while (iss >> v) { if (!value.empty()) value += " "; value += v; } } }
            }
            if (name == "Hash") {
                try { int mb = std::stoi(value); tt_resize(mb); } catch (...) {}
            }
        } else if (cmd == "isready") {
            std::cout << "readyok\n";
        } else if (cmd == "position") {
            if (g_search_thread.joinable()) { signal_stop(); g_search_thread.join(); }
            g_searching = false;
            g_history.clear();
            std::string what; iss >> what;
            if (what == "startpos") {
                g_board = Board();
                std::string mv; iss >> mv; // "moves"
                if (mv == "moves") apply_moves(g_board, line.substr(line.find("moves") + 5));
            } else if (what == "fen") {
                std::string fen;
                // leer hasta "moves" o final
                std::string token;
                while (iss >> token && token != "moves") fen += token + " ";
                g_board = Board(fen);
                if (token == "moves") apply_moves(g_board, line.substr(line.find("moves") + 5));
            }
        } else if (cmd == "go") {
            SearchLimits lim;
            lim.max_depth = 64;
            lim.time_ms = 0; // sin límite por defecto; se override con movetime/etc
            bool has_time = false;
            int64_t movetime = 0;
            int64_t wtime = 0, btime = 0, winc = 0, binc = 0;
            int movestogo = 0;
            std::string token;
            while (iss >> token) {
                if (token == "depth") { if (iss >> token) lim.max_depth = std::stoi(token); }
                else if (token == "movetime") { if (iss >> token) { movetime = std::stoll(token); has_time = true; } }
                else if (token == "wtime") { if (iss >> token) wtime = std::stoll(token); }
                else if (token == "btime") { if (iss >> token) btime = std::stoll(token); }
                else if (token == "winc") { if (iss >> token) winc = std::stoll(token); }
                else if (token == "binc") { if (iss >> token) binc = std::stoll(token); }
                else if (token == "movestogo") { if (iss >> token) movestogo = std::stoi(token); }
                else if (token == "infinite") { lim.time_ms = 0; lim.max_depth = 64; has_time = false; }
            }
            if (movetime > 0) { lim.time_ms = movetime; has_time = true; }
            else if (wtime > 0 || btime > 0) {
                int64_t mytime = (g_board.side == WHITE) ? wtime : btime;
                int64_t myinc = (g_board.side == WHITE) ? winc : binc;
                // Margen de maniobra: usa ~ el tiempo disponible / movimientos restantes
                int64_t budget = mytime / (movestogo > 0 ? movestogo : 30);
                budget += myinc;
                if (budget < 50) budget = 50;
                // dejar margen de seguridad y nunca exceder el reloj disponible
                int64_t max_allowed = std::max<int64_t>(10, mytime - 20);
                lim.time_ms = std::min<int64_t>(budget * 9 / 10, max_allowed);
                has_time = true;
            }
            if (!has_time) { lim.time_ms = 1000; lim.max_depth = 4; } // fallback seguro

            // Reportar PV en cada iteración (formato UCI "info ... pv ...")
            lim.on_info = [](int depth, Score score, uint64_t nodes, int64_t time_ms, const std::vector<Move>& pv) {
                std::cout << "info depth " << depth << " nodes " << nodes << " time " << time_ms << " ";
                if (score > MATE - 1000)      std::cout << "score mate " << ((MATE - score) / 2);
                else if (score < -MATE + 1000) std::cout << "score mate " << -((MATE + score) / 2);
                else                           std::cout << "score cp " << score;
                std::cout << " pv";
                for (Move m : pv) std::cout << " " << move_to_uci(m);
                std::cout << "\n";
            };

            if (g_search_thread.joinable()) { signal_stop(); g_search_thread.join(); }
            clear_stop();
            set_game_history(g_history);   // historial real para triple repetición
            g_searching = true;
            g_search_thread = std::thread(run_search, lim, g_board);
        } else if (cmd == "stop") {
            signal_stop();
            if (g_search_thread.joinable()) g_search_thread.join();
            g_searching = false;
            // bestmove ya se imprimió al terminar el hilo
        } else if (cmd == "quit" || cmd == "exit") {
            signal_stop();
            if (g_search_thread.joinable()) g_search_thread.join();
            break;
        } else if (cmd == "ucinewgame") {
            if (g_search_thread.joinable()) { signal_stop(); g_search_thread.join(); }
            g_searching = false;
            clear_stop();
            tt_clear();          // no arrastrar datos de la partida anterior
            g_board = Board();
            g_history.clear();
        }
        // otros comandos se ignoran
    }
    // Al cerrarse la entrada (EOF) o salir, asegurar que el hilo de búsqueda
    // termine ordenadamente y se imprima bestmove si está pendiente.
    signal_stop();
    if (g_search_thread.joinable()) g_search_thread.join();
    return 0;
}
