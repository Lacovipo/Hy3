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
#include <cstring>

#ifdef _WIN32
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

using namespace hy3;

static Board g_board;
static std::thread g_search_thread;
static std::atomic<bool> g_searching(false);
// La búsqueda SIEMPRE emite un 'bestmove' al terminar, cualquiera que sea el
// motivo de terminación, para que la GUI nunca se quede esperando. (Fix #1)
static Move g_best_move = 0;
static SearchResult g_last_result;
static std::vector<uint64_t> g_history;   // claves Zobrist de las posiciones jugadas

#ifdef _WIN32
// Adjunta el proceso a un Job Object para que cualquier proceso hijo muera con
// el padre (evita procesos huérfanos al cerrar el motor). Es una medida
// defensiva: el motor no lanza subprocesos, pero si algún día lo hace, no
// quedarán colgando. (Fix PARA_EL_AUTOR #5)
static void attach_job_object() {
    HANDLE job = CreateJobObjectA(NULL, NULL);
    if (!job) return;
    JOBOBJECT_EXTENDED_LIMIT_INFORMATION info;
    std::memset(&info, 0, sizeof(info));
    info.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
    if (!SetInformationJobObject(job, JobObjectExtendedLimitInformation, &info, sizeof(info))) {
        CloseHandle(job);
        return;
    }
    // Si ya estamos en un job (p.ej. un supervisor que nos lanza), no podemos
    // reasignar; lo ignoramos en silencio.
    if (!AssignProcessToJobObject(job, GetCurrentProcess()))
        CloseHandle(job);
}
#endif

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
    // SIEMPRE emitir exactamente una línea 'bestmove' al terminar la búsqueda,
    // sea cual sea el motivo (límite de tiempo, 'stop' o aborto por
    // 'position'/'setoption'/'quit'). Garantizarlo evita que la GUI se quede
    // esperando indefinidamente y pierda la partida por tiempo. (Fix #1)
    if (g_best_move != 0) {
        std::cout << "bestmove " << move_to_uci(g_best_move);
        // Sugerencia de ponder: segundo movimiento de la PV.
        if (g_last_result.ponder != 0)
            std::cout << " ponder " << move_to_uci(g_last_result.ponder);
        std::cout << std::endl;
    } else {
        std::cout << "bestmove 0000" << std::endl;
    }
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
#ifdef _WIN32
    attach_job_object();        // evita procesos hijos huérfanos al cerrar (Fix #5)
#endif
    std::string line;
    while (std::getline(std::cin, line)) {
        std::istringstream iss(line);
        std::string cmd;
        // Descarta tokens iniciales desconocidos hasta encontrar un comando
        // válido (p.ej. "joho isready" sigue produciendo "readyok"). (Fix #6)
        bool quit = false;
        while (iss >> cmd) {
        if (cmd == "uci") {
            std::cout << "id name " << ENGINE_ID << "\n";
            std::cout << "id author " << ENGINE_NAME << "\n";
            std::cout << "option name Hash type spin default 64 min 1 max 4096\n";
            std::cout << "option name Ponder type check default false\n";
            std::cout << "option name Clear Hash type button\n";
            std::cout << "uciok\n";
            break;
        } else if (cmd == "setoption") {
            // formato: setoption name <Nombre> [value <Valor>]
            std::string tok, name, value;
            while (iss >> tok) {
                if (tok == "name") { std::string w; while (iss >> w && w != "value") { if (!name.empty()) name += " "; name += w; } if (w == "value") { std::string v; while (iss >> v) { if (!value.empty()) value += " "; value += v; } } }
            }
            if (name == "Hash") {
                // CRÍTICO: redimensionar la TT mientras el hilo de búsqueda la
                // está leyendo liberaba la memoria bajo sus pies (use-after-free).
                // v1.6 detiene y espera a la búsqueda antes de tocar la tabla.
                if (g_search_thread.joinable()) { signal_stop(); g_search_thread.join(); g_searching = false; }
                try { int mb = std::stoi(value); tt_resize(mb); } catch (...) {}
            } else if (name == "Clear Hash") {
                if (g_search_thread.joinable()) { signal_stop(); g_search_thread.join(); g_searching = false; }
                tt_clear();
            }
            break;
        } else if (cmd == "isready") {
            std::cout << "readyok\n";
            break;
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
                // Validar el FEN antes de aplicarlo: si es inválido, conservar
                // la posición anterior y avisar. (Fix #8)
                std::string ferr;
                if (!validate_fen(fen, ferr)) {
                    std::cout << "info string Invalid FEN ignored: " << ferr << std::endl;
                } else {
                    g_board = Board(fen);
                    if (token == "moves") apply_moves(g_board, line.substr(line.find("moves") + 5));
                }
            }
            break;
        } else if (cmd == "go") {
            // Ignorar un 'go' que llegue con una búsqueda ya en curso: debe
            // haber exactamente un 'bestmove' por cada 'go'. (Fix #3)
            if (g_searching.load()) break;
            SearchLimits lim;
            lim.max_depth = 64;
            lim.time_ms = 0; // sin límite por defecto; se override con movetime/etc
            bool has_time = false;
            bool has_depth = false;      // el usuario pidió 'depth N'
            bool is_infinite = false;    // el usuario pidió 'infinite'
            bool is_ponder = false;      // el usuario pidió 'ponder'
            int64_t movetime = 0;
            int64_t wtime = 0, btime = 0, winc = 0, binc = 0;
            int movestogo = 0;
            std::string token;
            while (iss >> token) {
                if (token == "depth") { if (iss >> token) { lim.max_depth = std::stoi(token); has_depth = true; } }
                else if (token == "movetime") { if (iss >> token) { movetime = std::stoll(token); has_time = true; } }
                else if (token == "wtime") { if (iss >> token) wtime = std::stoll(token); }
                else if (token == "btime") { if (iss >> token) btime = std::stoll(token); }
                else if (token == "winc") { if (iss >> token) winc = std::stoll(token); }
                else if (token == "binc") { if (iss >> token) binc = std::stoll(token); }
                else if (token == "movestogo") { if (iss >> token) movestogo = std::stoi(token); }
                else if (token == "infinite") { is_infinite = true; }
                else if (token == "ponder")   { is_ponder = true; }
                else if (token == "mate")     { if (iss >> token) { lim.max_depth = std::stoi(token) * 2; has_depth = true; } }
                else if (token == "nodes")    { if (iss >> token) { try { lim.max_nodes = std::stoll(token); } catch (...) {} } }
                else if (token == "searchmoves") { /* no soportado: se ignora */ }
            }
            if (is_infinite) {
                // 'infinite': buscar hasta 'stop'. Sin límite de tiempo ni de
                // profundidad efectiva.
                lim.time_ms = 0;
                if (!has_depth) lim.max_depth = 64;
                has_time = false;
            }
            else if (movetime > 0) {
                // Reservar margen para no sobrepasar el movetime exacto que
                // exige el árbitro. Tope DURO = movetime - 5 ms. (Fix #2)
                lim.time_ms = std::max<int64_t>(5, movetime - 25);
                lim.max_time_ms = std::max<int64_t>(5, movetime - 5);
                has_time = true;
            }
            else if (wtime > 0 || btime > 0) {
                int64_t mytime = (g_board.side == WHITE) ? wtime : btime;
                int64_t myinc = (g_board.side == WHITE) ? winc : binc;
                // Reparto del tiempo a partir del reloj REAL que envía la GUI.
                int64_t moves_left = (movestogo > 0) ? movestogo
                                   : (mytime < 20000 ? 40 : 30);
                int64_t budget = mytime / moves_left + myinc;
                // Nunca gastar más de 1/3 del tiempo restante en una jugada
                // (reserva para las siguientes). (Fix #4)
                int64_t cap = (mytime * 33) / 100;
                // Techo duro: nunca superar el reloj real menos un margen => no
                // caer en bandera jamás.
                int64_t safe = std::max<int64_t>(1, mytime - 10);
                int64_t soft = std::min(budget, cap);
                if (soft < 20) soft = 20;
                int64_t hard = std::min(safe, cap * 4);
                if (hard < soft + 20) hard = soft + 20;
                if (soft > safe) soft = safe;
                lim.time_ms = soft;
                lim.max_time_ms = hard;
                has_time = true;
            }
            // 'go' sin límites: búsqueda INFINITA hasta 'stop' (especificación
            // UCI). Antes devolvía una jugada sola. (Fix #7)
            if (!has_time && !has_depth && !is_infinite) {
                lim.time_ms = 0;
                lim.max_depth = 64;
            }
            lim.ponder = is_ponder;

            // Reportar PV en cada iteración (formato UCI "info ... pv ...")
            lim.on_info = [](int depth, Score score, uint64_t nodes, int64_t time_ms, const std::vector<Move>& pv) {
                std::cout << "info depth " << depth << " nodes " << nodes << " time " << time_ms;
                // Nodos por segundo (los GUIs lo esperan).
                if (time_ms > 0) std::cout << " nps " << (nodes * 1000 / (uint64_t)time_ms);
                std::cout << " ";
                // Notación de mate UCI: 'mate N' cuenta JUGADAS COMPLETAS y el
                // mate inmediato es 'mate 1', no 'mate 0'. v1.5 emitía
                // (MATE - score)/2, que daba 0 para un mate en 1.
                if (score > MATE - 1000)       std::cout << "score mate " << ((MATE - score + 1) / 2);
                else if (score < -MATE + 1000) std::cout << "score mate " << -((MATE + score + 1) / 2);
                else                           std::cout << "score cp " << score;
                std::cout << " pv";
                for (Move m : pv) std::cout << " " << move_to_uci(m);
                std::cout << std::endl;   // los GUIs necesitan vaciado inmediato
            };

            if (g_search_thread.joinable()) { signal_stop(); g_search_thread.join(); }
            clear_stop();
            set_game_history(g_history);   // historial real para triple repetición
            g_searching = true;
            g_search_thread = std::thread(run_search, lim, g_board);
            break;
        } else if (cmd == "stop") {
            signal_stop();
            if (g_search_thread.joinable()) g_search_thread.join();
            g_searching = false;
            // bestmove ya se imprimió al terminar el hilo (no se suprime)
            break;
        } else if (cmd == "ponderhit") {
            // El rival jugó la jugada que estábamos ponderando: el tiempo de
            // ponder fue gratis y el reloj propio empieza a correr ahora. (Fix #1)
            ponder_hit();
            break;
        } else if (cmd == "tree") {
            // Extensión (no UCI) pedida en la revisión humana: traza el árbol
            // de la raíz. Uso: 'tree on [prof]' / 'tree off'.
            std::string arg; iss >> arg;
            if (arg == "off") { set_tree_debug(false, 0); std::cout << "info string tree debug OFF\n"; }
            else {
                int d = 3; std::string dp;
                if (iss >> dp) { try { d = std::stoi(dp); } catch (...) {} }
                set_tree_debug(true, d);
                std::cout << "info string tree debug ON (max depth " << d << ")\n";
            }
            break;
        } else if (cmd == "quit" || cmd == "exit") {
            if (g_search_thread.joinable()) { signal_stop(); g_search_thread.join(); }
            quit = true;
            break;
        } else if (cmd == "ucinewgame") {
            if (g_search_thread.joinable()) { signal_stop(); g_search_thread.join(); }
            g_searching = false;
            clear_stop();
            tt_clear();          // no arrastrar datos de la partida anterior
            g_board = Board();
            g_history.clear();
            break;
        } else {
            // token inicial desconocido: ignorarlo y seguir escaneando la línea
            continue;
        }
        }
        if (quit) break;
    }
    // Al cerrarse la entrada (EOF) o salir, asegurar que el hilo de búsqueda
    // termine ordenadamente.
    signal_stop();
    if (g_search_thread.joinable()) g_search_thread.join();
    return 0;
}
