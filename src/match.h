#ifndef MATCH_H
#define MATCH_H

// Plays `games` MCTS-vs-random games (alternating colours) and prints results.
typedef struct {
    int games;
    int watch;          // print every move and the board
    int weak_iters;     // 0 = opponent is the random mover; > 0 = plain MCTS with this many iterations
} MatchConfig;

// Plays cfg->games engine-vs-opponent games (alternating colours) and prints results.
// The engine uses the current search settings (mcts_iterations, swindle_pct, ...).
// The weak opponent is plain MCTS: no swindle, no beauty, no book.
void run_match(const MatchConfig* cfg);

#endif
