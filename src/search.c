// search.c - Move selection
//
// Placeholder engine. The real search (negamax + alpha-beta + evaluation)
// belongs here; for now we pick a uniformly random legal move so the UCI
// layer has something instant and always-legal to return.
#include <stdlib.h>
#include "search.h"
#include "validation.h"
#include "randommove.h"

// NOTE: kept thread-unsafe on purpose. When a real, time-limited search lands
// here it will run on a worker thread and consult a shared "stop" flag; until
// then this is called synchronously from the UCI loop and needs no locking.
Move find_best_move(void) {
    return get_random_move(side_to_move);
}
