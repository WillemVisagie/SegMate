// main.c - Entry point and launch-mode dispatch
//
// Mode selection (first match wins):
//   --uci / -u          Always speak UCI on stdin.
//   --referee / -r      Always launch the interactive referee menu.
//   --help / -h         Show usage.
//   (no flag)           Auto-detect: if stdin is a terminal, run the referee
//                       (interactive use); otherwise run UCI (a GUI is piping
//                       commands). This lets any UCI GUI launch the bare
//                       executable without needing to pass arguments.
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include "chess.h"
#include "fen.h"
#include "book.h"
#include "match.h"
#include "perft.h"
#include "platform.h"
#include "referee.h"
#include "search.h"
#include "uci.h"

static void print_usage(void) {
    printf("SegMate chess engine\n");
    printf("Usage:\n");
    printf("  segmate              Interactive referee menu (auto-detected\n");
    printf("                       when stdin is a terminal), or UCI when stdin\n");
    printf("                       is piped (e.g. by a chess GUI)\n");
    printf("  segmate --uci        Speak the UCI protocol on stdin\n");
    printf("  segmate --referee    Force the interactive referee menu\n");
    printf("  segmate --match [N] [--watch] [--weak ITERS] [--iters N] [--swindle P] [--beauty P] [--nobook]\n                       Play N games vs the random mover (or plain MCTS with ITERS iterations)\n");
    printf("  segmate --perft D [FEN]  Count legal-move tree leaves to depth D\n");
    printf("  segmate --help       Show this help\n");
}

int main(int argc, char** argv) {
    srand((unsigned)time(NULL) ^ ((unsigned)GETPID() << 16)); // pid: engines started in the same second must differ
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--uci") == 0 || strcmp(argv[i], "-u") == 0) {
            uci_main();
            return 0;
        }
        if (strcmp(argv[i], "--referee") == 0 || strcmp(argv[i], "-r") == 0) {
            start_match();
            return 0;
        }
        if (strcmp(argv[i], "--match") == 0) {
            MatchConfig cfg = {10, 0, 0};
            for (int j = i + 1; j < argc; j++) {
                if (strcmp(argv[j], "--watch") == 0) cfg.watch = 1;
                else if (strcmp(argv[j], "--weak") == 0 && j + 1 < argc) cfg.weak_iters = atoi(argv[++j]);
                else if (strcmp(argv[j], "--iters") == 0 && j + 1 < argc) mcts_iterations = atoi(argv[++j]);
                else if (strcmp(argv[j], "--swindle") == 0 && j + 1 < argc) swindle_pct = atoi(argv[++j]);
                else if (strcmp(argv[j], "--beauty") == 0 && j + 1 < argc) beauty_pct = atoi(argv[++j]);
                else if (strcmp(argv[j], "--nobook") == 0) book_enabled = 0;
                else cfg.games = atoi(argv[j]);
            }
            run_match(&cfg);
            return 0;
        }
        if (strcmp(argv[i], "--perft") == 0) { // --perft DEPTH [FEN]
            if (i + 1 >= argc) { print_usage(); return 1; }
            if (i + 2 < argc) {
                if (set_board_from_fen(argv[i + 2]) != 0) { fprintf(stderr, "bad FEN\n"); return 1; }
            } else {
                init_board();
            }
            perft_divide(atoi(argv[i + 1]));
            return 0;
        }
        if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
            print_usage();
            return 0;
        }
    }

    // No explicit flag: auto-detect from stdin. A TTY means a human at the
    // terminal (referee); a non-TTY means piped input, i.e. a UCI GUI.
    if (IS_TTY(fileno(stdin))) {
        start_match();
    } else {
        uci_main();
    }
    return 0;
}
