#ifndef FEN_H
#define FEN_H

// Parses a FEN string and sets all game state (board, castling rights,
// en passant target, side to move, halfmove clock, fullmove number).
// Returns 0 on success, -1 on malformed input.
int set_board_from_fen(const char* fen);

// Generates the FEN string describing the current state into `out`.
// `out` must be at least 90 bytes.
void board_to_fen(char* out);

#endif // FEN_H
