#ifndef SEARCH_H
#define SEARCH_H

#include "chess.h"

// Number of MCTS iterations per move.
extern int mcts_iterations;

// The final move is chosen by a reply check (best value against about the
// best reply among well-explored moves), not just the most visited.
// Personality knob (UCI option Beauty; 0 disables it):
//  beauty_pct: max bonus x1000 added to a playout result for the root side's
//    sacrifices that pay off, under-promotions that don't lose, and mates with
//    few pieces. It never changes win/draw/loss ordering at sane values.
extern int beauty_pct;

// Playout policy, for A/B tests against older versions: 0 = uniform random to
// the end; 1 = biased toward captures/promotions with an early material cutoff;
// 2 (default) = like 1 but stops after playout_plies and scores by evaluate().
extern int smart_playouts;
extern int playout_plies;
// Steer the tree by the eval: children get priors from it (PUCT selection).
extern int use_priors;

typedef struct {
    double time_ms;   // > 0: stop after this many milliseconds
    long iterations;  // > 0: stop after this many MCTS iterations ("nodes")
    int infinite;     // search until search_request_stop()
    int info;         // print UCI "info" lines while searching
} SearchLimits;       // all zero = fixed default of mcts_iterations

// Cooperative stop for a search running on another thread. Clear the flag
// before starting a search; the search polls it and returns its best move so far.
void search_clear_stop(void);
void search_request_stop(void);
int search_stop_requested(void);

// Searches the current position within the limits (always at least one
// iteration) and returns the best move. Not reentrant: it uses the global board
// and restores it before returning, so nothing else may touch it meanwhile.
Move search_best_move(const SearchLimits* lim);

// Returns the engine's chosen move (MCTS) for the current position, leaving
// the global board state unchanged. This is the single seam between the
// protocol layer (UCI / referee) and move selection; get_random_move() in
// randommove.h remains available as a baseline opponent.
//
// Returns a Move with src_c == -1 when there is no legal move.
Move find_best_move(void);

#endif // SEARCH_H
