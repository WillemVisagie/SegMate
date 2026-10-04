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
#ifdef _WIN32
#include <io.h>
#define IS_TTY(fd) _isatty(fd)
#else
#include <unistd.h>
#define IS_TTY(fd) isatty(fd)
#endif
#include <stdlib.h>
#include <time.h>
#include "match.h"
#include "referee.h"
#include "uci.h"

static void print_usage(void) {
    printf("SegMate chess engine\n");
    printf("Usage:\n");
    printf("  segmate              Interactive referee menu (auto-detected\n");
    printf("                       when stdin is a terminal), or UCI when stdin\n");
    printf("                       is piped (e.g. by a chess GUI)\n");
    printf("  segmate --uci        Speak the UCI protocol on stdin\n");
    printf("  segmate --referee    Force the interactive referee menu\n");
    printf("  segmate --match [N] [--watch]  Play N MCTS-vs-random games (default 10), optionally printing every move\n");
    printf("  segmate --help       Show this help\n");
}

int main(int argc, char** argv) {
    srand((unsigned)time(NULL));
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
            int watch = 0, games = 10;
            for (int j = i + 1; j < argc; j++) {
                if (strcmp(argv[j], "--watch") == 0) watch = 1;
                else games = atoi(argv[j]);
            }
            run_match(games, watch);
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
