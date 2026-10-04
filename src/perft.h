#ifndef PERFT_H
#define PERFT_H

// Counts leaf nodes of the legal-move tree from the current position.
// Leaves the global position unchanged.
unsigned long long perft(int depth);

// Like perft, but prints the per-root-move counts (for diffing against a
// reference engine) and the total.
unsigned long long perft_divide(int depth);

#endif
