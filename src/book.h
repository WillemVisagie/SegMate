#ifndef BOOK_H
#define BOOK_H

#include "chess.h"

// 1 = consult the opening book before searching (UCI option "OwnBook").
extern int book_enabled;

// If the current position is in the book, stores a randomly chosen book move
// in *out and returns 1; otherwise returns 0. Positions reached by a different
// move order (transpositions) are recognised. Moves are weighted by how many
// named opening lines use them (gambit lines count extra).
int book_probe(Move* out);

#endif // BOOK_H
