#ifndef SEARCH_H
#define SEARCH_H

#include "chess.h"

// Number of MCTS iterations per move.
extern int mcts_iterations;

// Returns the engine's chosen move (MCTS) for the current position, leaving
// the global board state unchanged. This is the single seam between the
// protocol layer (UCI / referee) and move selection; get_random_move() in
// randommove.h remains available as a baseline opponent.
//
// Returns a Move with src_c == -1 when there is no legal move.
Move find_best_move(void);

#endif // SEARCH_H
