// randommove.c - AI that selects a random legal move
#include <stdlib.h>
#include <time.h>
#include "randommove.h"
#include "validation.h"

Move get_random_move(int is_white) {
    MoveList list;
    get_legal_moves(is_white, &list);

    if (list.count == 0) {
        // Fallback if no moves are available (Game Over state)
        return (Move){-1, -1, -1, -1, '\0'};
    }

    // Pick a random index from the available moves
    int random_index = rand() % list.count;
    return list.moves[random_index];
}