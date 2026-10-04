// match.c - MCTS vs random-mover self-play, to sanity-check playing strength
#include <stdio.h>
#include "chess.h"
#include "match.h"
#include "randommove.h"
#include "search.h"
#include "validation.h"

#define MAX_PLIES 300

void run_match(int games, int watch) {
    int mcts_w = 0, rand_w = 0, draws = 0;
    for (int g = 0; g < games; g++) {
        int mcts_white = g % 2 == 0; // alternate colours
        init_board();
        int status = 2;
        if (watch) printf("\n=== Game %d: MCTS (%s) vs Random ===\n", g + 1, mcts_white ? "White" : "Black");
        for (int ply = 0; ply < MAX_PLIES; ply++) {
            status = check_if_over(side_to_move);
            if (status != 2) break;
            int mcts_turn = (side_to_move == 1) == mcts_white;
            Move m = mcts_turn ? find_best_move() : get_random_move(side_to_move);
            if (watch) {
                char buf[16];
                move_to_string(m, buf);
                printf("\n%d. %s (%s) plays %s\n", ply / 2 + 1, side_to_move ? "White" : "Black",
                       mcts_turn ? "MCTS" : "Random", buf);
            }
            execute_move(m);
            if (watch) { print_board(); fflush(stdout); }
        }
        const char* res = "draw";
        if (status == 1 || status == -1) {
            int white_won = status == 1;
            if (white_won == mcts_white) { mcts_w++; res = "MCTS wins"; }
            else { rand_w++; res = "random wins"; }
        } else draws++;
        printf("game %d (MCTS as %s): %s\n", g + 1, mcts_white ? "White" : "Black", res);
        fflush(stdout);
    }
    printf("MCTS %d - Random %d - Draws %d\n", mcts_w, rand_w, draws);
}
