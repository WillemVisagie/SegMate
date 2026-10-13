#ifndef SEARCH_H
#define SEARCH_H

#include "chess.h"

// Number of MCTS iterations per move.
extern int mcts_iterations;

// Personality knobs (UCI options Swindle / Beauty); 0 disables each, giving
// plain MCTS.
//  swindle_pct: base temperature x100 of the fallible-opponent model. After
//    the search, among well-explored root moves, it picks the one with the best
//    expected value if the opponent picks replies by softmax over their value at
//    this temperature instead of always the best one, so it prefers moves where
//    only a few replies hold. It adapts: cooler when winning, hotter when losing.
//  beauty_pct: max bonus x1000 added to a playout result for the root side's
//    sacrifices that pay off, under-promotions that don't lose, and mates with
//    few pieces. It never changes win/draw/loss ordering at sane values.
extern int swindle_pct;
extern int beauty_pct;

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
