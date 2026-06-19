// main.c - Entry point and launch-mode dispatch
//
// No arguments (default): the interactive referee menu (Human/AI play modes).
// --uci / -u             : speak the UCI protocol on stdin for chess GUIs.
#include <stdio.h>
#include <string.h>
#include "referee.h"
#include "uci.h"

int main(int argc, char** argv) {
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--uci") == 0 || strcmp(argv[i], "-u") == 0) {
            uci_main();
            return 0;
        }
        if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
            printf("SegMate chess engine\n");
            printf("Usage:\n");
            printf("  segmate            Interactive referee menu (default)\n");
            printf("  segmate --uci      Speak the UCI protocol on stdin\n");
            printf("  segmate --help     Show this help\n");
            return 0;
        }
    }
    start_match();
    return 0;
}
