#ifndef MATCH_H
#define MATCH_H

// Plays `games` MCTS-vs-random games (alternating colours) and prints results.
typedef struct {
    int games;
    int watch;          // print every move and the board
    int weak_iters;     // 0 = opponent is the random mover; > 0 = plain MCTS with this many iterations
    int weak_playouts;  // weak opponent's playout level (smart_playouts), e.g. an older version's
    double movetime_ms; // > 0: both sides search this long per move instead of a fixed iteration count
} MatchConfig;

// Plays cfg->games engine-vs-opponent games (alternating colours) and prints results.
// The engine uses the current search settings (mcts_iterations, swindle_pct, ...).
// The weak opponent is plain MCTS: no swindle, no beauty, no book.
void run_match(const MatchConfig* cfg);

#endif
