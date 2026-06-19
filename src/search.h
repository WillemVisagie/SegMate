#ifndef SEARCH_H
#define SEARCH_H

#include "chess.h"

// Returns the engine's chosen move for the current position.
//
// This is the single seam between the protocol layer (UCI / referee) and the
// move-selection logic. Today it delegates to the random mover. A future
// negamax/alpha-beta engine should replace only the body of this function —
// UCI threading and time management will plug in here alongside it.
//
// Returns a Move with src_c == -1 when there is no legal move.
Move find_best_move(void);

#endif // SEARCH_H
