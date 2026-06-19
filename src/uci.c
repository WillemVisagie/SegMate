// uci.c - Universal Chess Interface protocol implementation
//
// Speaks UCI on stdin/stdout so the engine can be driven by chess GUIs
// (Cute Chess, Arena, pychess, lichess-bot, etc.). Command reference:
// http://wbec-ridderkerk.nl/html/UCIProtocol.html
//
// Move selection is synchronous today (random mover is instant), so `go`
// returns `bestmove` immediately. When a real, time-limited search lands in
// search.c, the `go`/`stop` handling here will gain a worker thread and the
// time/depth fields below will actually be honoured.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "chess.h"
#include "validation.h"
#include "fen.h"
#include "search.h"
#include "uci.h"

#define ENGINE_NAME    "SegMate"
#define ENGINE_AUTHOR  "Willem Visagie"
#define LINE_MAX       4096

// Emits the UCI identification handshake.
static void send_id(void) {
    printf("id name %s\n", ENGINE_NAME);
    printf("id author %s\n", ENGINE_AUTHOR);
    printf("option name Random Move type check default true\n");
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

// Handles `go`. Time/depth/nodes fields are parsed and ignored for now because
// the random mover is instantaneous; they will be consumed once a real search
// runs on a worker thread.
static void handle_go(const char* args) {
    (void)args; // No timing logic yet — see file header.

    // If the side to move has no legal moves, the GUI has usually already
    // signalled game over; reply with the conventional null move regardless.
    MoveList list;
    get_legal_moves(side_to_move, &list);
    if (list.count == 0) {
        printf("bestmove (none)\n");
        fflush(stdout);
        return;
    }

    Move m = find_best_move();
    if (m.src_c == -1) {
        printf("bestmove (none)\n");
    } else {
        char buf[8];
        move_to_uci_string(m, buf);
        printf("bestmove %s\n", buf);
    }
    fflush(stdout);
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
        init_board();
    } else if (strcmp(cmd, "setoption") == 0) {
        // We expose "Random Move" but accept any setoption silently; no real
        // options are wired up yet.
    } else if (strcmp(cmd, "position") == 0) {
        handle_position(args);
    } else if (strcmp(cmd, "go") == 0) {
        handle_go(args);
    } else if (strcmp(cmd, "stop") == 0) {
        // No asynchronous search yet; nothing to stop.
    } else if (strcmp(cmd, "ponderhit") == 0) {
        // No pondering yet.
    } else if (strcmp(cmd, "debug") == 0) {
        // "debug on"/"debug off" — accepted, no extra output implemented.
    } else if (strcmp(cmd, "register") == 0) {
        printf("registration later\n");
        fflush(stdout);
    } else if (strcmp(cmd, "quit") == 0) {
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
}
