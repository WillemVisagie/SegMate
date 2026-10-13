// match.c - MCTS vs random-mover self-play, to sanity-check playing strength
#include <stdio.h>
#include "chess.h"
#include "book.h"
#include "match.h"
#include "randommove.h"
#include "search.h"
#include "validation.h"

#define MAX_PLIES 300

static Move engine_move(const MatchConfig* cfg) {
    SearchLimits lim = {cfg->movetime_ms, 0, 0, 0};
    return search_best_move(&lim);
}

static Move weak_move(const MatchConfig* cfg) {
    int sw = swindle_pct, bt = beauty_pct, bk = book_enabled, it = mcts_iterations, sp = smart_playouts;
    swindle_pct = 0; beauty_pct = 0; book_enabled = 0; mcts_iterations = cfg->weak_iters;
    smart_playouts = cfg->weak_playouts;
    Move m = engine_move(cfg);
    swindle_pct = sw; beauty_pct = bt; book_enabled = bk; mcts_iterations = it; smart_playouts = sp;
    return m;
}

void run_match(const MatchConfig* cfg) {
    int games = cfg->games, watch = cfg->watch;
    const char* opp = cfg->weak_iters > 0 ? "MCTS-weak" : "Random";
    int mcts_w = 0, rand_w = 0, draws = 0;
    for (int g = 0; g < games; g++) {
        int mcts_white = g % 2 == 0; // alternate colours
        init_board();
        int status = 2;
        if (watch) printf("\n=== Game %d: MCTS (%s) vs %s ===\n", g + 1, mcts_white ? "White" : "Black", opp);
        for (int ply = 0; ply < MAX_PLIES; ply++) {
            status = check_if_over(side_to_move);
            if (status != 2) break;
            int mcts_turn = (side_to_move == 1) == mcts_white;
            Move m = mcts_turn ? engine_move(cfg)
                 : cfg->weak_iters > 0 ? weak_move(cfg) : get_random_move(side_to_move);
            if (watch) {
                char buf[16];
                move_to_string(m, buf);
                printf("\n%d. %s (%s) plays %s\n", ply / 2 + 1, side_to_move ? "White" : "Black",
                       mcts_turn ? "MCTS" : opp, buf);
            }
            execute_move(m);
            if (watch) { print_board(); fflush(stdout); }
        }
        const char* res = "draw";
        if (status == 1 || status == -1) {
            int white_won = status == 1;
            if (white_won == mcts_white) { mcts_w++; res = "MCTS wins"; }
            else { rand_w++; res = "opponent wins"; }
        } else draws++;
        printf("game %d (MCTS as %s): %s\n", g + 1, mcts_white ? "White" : "Black", res);
        fflush(stdout);
    }
    printf("MCTS %d - %s %d - Draws %d\n", mcts_w, opp, rand_w, draws);
}
