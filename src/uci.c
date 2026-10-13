// uci.c - Universal Chess Interface protocol implementation
//
// Speaks UCI on stdin/stdout so the engine can be driven by chess GUIs
// (Cute Chess, Arena, pychess, lichess-bot, etc.). Command reference:
// http://wbec-ridderkerk.nl/html/UCIProtocol.html
//
// `go` runs the search on a worker thread so `stop` (and `quit` during an
// infinite search) can interrupt it. While a search runs, only the worker may
// touch the global board, so any command that changes the position first stops
// and joins the search. Plain `go` (no limits) is not "infinite" here: it runs
// a fixed number of iterations, which keeps piped `go` + `quit` scripts working.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "book.h"
#include "chess.h"
#include "validation.h"
#include "fen.h"
#include "platform.h"
#include "search.h"
#include "uci.h"

#define ENGINE_NAME    "SegMate"
#define ENGINE_AUTHOR  "Willem Visagie"
#define LINE_MAX       4096
#define MOVE_OVERHEAD_MS 30   // safety margin for I/O and thread start-up
#define DEFAULT_MOVES_TO_GO 30

// Emits the UCI identification handshake.
static void send_id(void) {
    printf("id name %s\n", ENGINE_NAME);
    printf("id author %s\n", ENGINE_AUTHOR);
    printf("option name OwnBook type check default true\n");
    printf("option name Swindle type spin default 40 min 0 max 100\n");
    printf("option name Beauty type spin default 50 min 0 max 100\n");
    printf("uciok\n");
    fflush(stdout);
}

// Sets up the position from a `position ...` command's argument tail.
// `args` is everything after the keyword "position".
static void handle_position(const char* args) {
    if (args == NULL) return;

    // Locate where the optional `moves` list begins; everything before it is
    // either "startpos" or "fen <6 fields>".
    const char* moves_kw = strstr(args, " moves");
    size_t setup_len;
    const char* moves_str = NULL;
    if (moves_kw != NULL) {
        setup_len = (size_t)(moves_kw - args);
        moves_str = moves_kw + 6; // skip " moves"
        while (*moves_str == ' ') moves_str++;
    } else {
        setup_len = strlen(args);
    }

    char setup[256];
    if (setup_len >= sizeof(setup)) setup_len = sizeof(setup) - 1;
    strncpy(setup, args, setup_len);
    setup[setup_len] = '\0';

    // Trim trailing whitespace.
    while (setup_len > 0 && (setup[setup_len - 1] == ' ' || setup[setup_len - 1] == '\t')) {
        setup[--setup_len] = '\0';
    }

    if (strncmp(setup, "startpos", 8) == 0) {
        init_board();
    } else if (strncmp(setup, "fen", 3) == 0) {
        // Skip "fen" and any spaces to reach the FEN body.
        const char* fen_body = setup + 3;
        while (*fen_body == ' ') fen_body++;
        if (set_board_from_fen(fen_body) != 0) {
            // Malformed FEN: fall back to the standard start position so the
            // engine never crashes on bad GUI input.
            init_board();
        }
    } else {
        init_board();
    }

    // Apply the move list, if any. Each move is in UCI form (e2e4 / e7e8q);
    // parse_move already accepts that format, and execute_move advances every
    // piece of state (castling, en passant, 50-move, side, fullmove, history).
    if (moves_str != NULL && *moves_str != '\0') {
        char moves[1024];
        strncpy(moves, moves_str, sizeof(moves) - 1);
        moves[sizeof(moves) - 1] = '\0';

        char* saveptr = NULL;
        char* tok = strtok_r(moves, " ", &saveptr);
        while (tok != NULL) {
            Move m = parse_move(tok);
            if (m.src_c == -1 || m.src_r == -1 || m.dst_c == -1 || m.dst_r == -1) {
                break; // Unparseable move: stop applying further moves.
            }
            // Validate legality against the current side to move before playing,
            // so an out-of-sync GUI can't desync our board.
            if (!check_if_legal(m, side_to_move)) {
                break;
            }
            execute_move(m);
            tok = strtok_r(NULL, " ", &saveptr);
        }
    }
}

typedef struct {
    long wtime, btime, winc, binc, movetime, movestogo, nodes; // -1 = not given
    int infinite;
} GoParams;

static void parse_go(const char* args, GoParams* g) {
    *g = (GoParams){-1, -1, -1, -1, -1, -1, -1, 0};
    if (args == NULL) return;
    char buf[LINE_MAX];
    strncpy(buf, args, sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = '\0';
    char* save = NULL;
    for (char* tok = strtok_r(buf, " \t", &save); tok; tok = strtok_r(NULL, " \t", &save)) {
        long* field = NULL;
        if (!strcmp(tok, "wtime")) field = &g->wtime;
        else if (!strcmp(tok, "btime")) field = &g->btime;
        else if (!strcmp(tok, "winc")) field = &g->winc;
        else if (!strcmp(tok, "binc")) field = &g->binc;
        else if (!strcmp(tok, "movetime")) field = &g->movetime;
        else if (!strcmp(tok, "movestogo")) field = &g->movestogo;
        else if (!strcmp(tok, "nodes")) field = &g->nodes;
        else if (!strcmp(tok, "infinite")) g->infinite = 1;
        else if (!strcmp(tok, "depth")) strtok_r(NULL, " \t", &save); // MCTS has no depth; accepted, ignored
        if (field) {
            char* v = strtok_r(NULL, " \t", &save);
            if (v) *field = atol(v);
        }
    }
}

// Milliseconds to spend on this move, or 0 if the GUI gave no time information.
static double time_budget_ms(const GoParams* g) {
    if (g->movetime >= 0) {
        double b = g->movetime - MOVE_OVERHEAD_MS;
        return b < 1 ? 1 : b;
    }
    long remaining = side_to_move ? g->wtime : g->btime;
    long inc = side_to_move ? g->winc : g->binc;
    if (remaining < 0) return 0;
    if (inc < 0) inc = 0;
    long moves = g->movestogo > 0 ? g->movestogo + 1 : DEFAULT_MOVES_TO_GO;
    double b = (double)remaining / moves + inc / 2.0;
    double max = remaining - 2.0 * MOVE_OVERHEAD_MS; // never plan to flag
    if (b > max) b = max;
    return b < 5 ? 5 : b;
}

static plat_thread worker;
static int searching = 0;
static SearchLimits limits;

static void* worker_main(void* arg) {
    (void)arg;
    Move m = search_best_move(&limits);
    // `go infinite` must not answer until told to stop, even if the search ended early.
    if (limits.infinite) while (!search_stop_requested()) plat_sleep_ms(1);
    if (m.src_c == -1) {
        printf("bestmove (none)\n");
    } else {
        char buf[8];
        move_to_uci_string(m, buf);
        printf("bestmove %s\n", buf);
    }
    fflush(stdout);
    return NULL;
}

// Interrupts the running search (if any) and waits for its bestmove to be printed.
static void stop_search(void) {
    if (!searching) return;
    search_request_stop();
    plat_thread_join(worker);
    searching = 0;
}

// Waits for the running search to finish on its own limits (infinite ones are stopped).
static void finish_search(void) {
    if (searching && !limits.infinite) {
        plat_thread_join(worker);
        searching = 0;
    }
    stop_search();
}

static void handle_go(const char* args) {
    stop_search();

    // If the side to move has no legal moves, the GUI has usually already
    // signalled game over; reply with the conventional null move regardless.
    MoveList list;
    get_legal_moves(side_to_move, &list);
    if (list.count == 0) {
        printf("bestmove (none)\n");
        fflush(stdout);
        return;
    }

    GoParams g;
    parse_go(args, &g);
    limits = (SearchLimits){0, 0, g.infinite, 1};
    if (!g.infinite) {
        limits.time_ms = time_budget_ms(&g);
        if (g.nodes > 0) limits.iterations = g.nodes;
    }
    search_clear_stop();
    searching = 1;
    if (plat_thread_create(&worker, worker_main, NULL) != 0) {
        searching = 0;
        worker_main(NULL); // no thread available: search synchronously
    }
}

// Returns 0 to keep the loop running, 1 to stop (on "quit").
static int handle_command(const char* line) {
    // Skip leading whitespace.
    while (*line == ' ' || *line == '\t') line++;
    if (*line == '\0' || *line == '\n') return 0;

    // Tokenize: first word is the command, the rest is its argument tail.
    char buf[LINE_MAX];
    strncpy(buf, line, sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = '\0';
    buf[strcspn(buf, "\r\n")] = '\0';

    char* saveptr = NULL;
    char* cmd = strtok_r(buf, " \t", &saveptr);
    if (cmd == NULL) return 0;
    char* args = strtok_r(NULL, "", &saveptr); // remainder, if any
    if (args != NULL) {
        while (*args == ' ' || *args == '\t') args++;
    }

    if (strcmp(cmd, "uci") == 0) {
        send_id();
    } else if (strcmp(cmd, "isready") == 0) {
        printf("readyok\n");
        fflush(stdout);
    } else if (strcmp(cmd, "ucinewgame") == 0) {
        stop_search();
        init_board();
    } else if (strcmp(cmd, "setoption") == 0) {
        stop_search();
        // setoption name <OwnBook|Swindle|Beauty> value <v>; other options are ignored.
        const char* value = args ? strstr(args, "value") : NULL;
        if (args != NULL && value != NULL) {
            if (strstr(args, "OwnBook") != NULL) book_enabled = strstr(value, "false") == NULL;
            else if (strstr(args, "Swindle") != NULL) swindle_pct = atoi(value + 5);
            else if (strstr(args, "Beauty") != NULL) beauty_pct = atoi(value + 5);
        }
    } else if (strcmp(cmd, "position") == 0) {
        stop_search();
        handle_position(args);
    } else if (strcmp(cmd, "go") == 0) {
        handle_go(args);
    } else if (strcmp(cmd, "stop") == 0) {
        stop_search();
    } else if (strcmp(cmd, "ponderhit") == 0) {
        // No pondering yet.
    } else if (strcmp(cmd, "debug") == 0) {
        // "debug on"/"debug off" — accepted, no extra output implemented.
    } else if (strcmp(cmd, "register") == 0) {
        printf("registration later\n");
        fflush(stdout);
    } else if (strcmp(cmd, "quit") == 0) {
        finish_search();
        return 1;
    }
    // Unknown commands are ignored, per the UCI spec.
    return 0;
}

void uci_main(void) {
    init_board();
    setvbuf(stdout, NULL, _IONBF, 0); // Unbuffered: GUIs parse line by line.

    char line[LINE_MAX];
    while (fgets(line, sizeof(line), stdin) != NULL) {
        if (handle_command(line)) break;
    }
    finish_search(); // stdin closed mid-search: let a bounded search answer
}
