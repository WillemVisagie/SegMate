#ifndef CHESS_H
#define CHESS_H

extern char board[8][8];
extern int move_count;
extern int halfmove_clock;
extern int fullmove_number; // Increments after Black's move (starts at 1)

// Side to move: 1 = White, 0 = Black
extern int side_to_move;

// Castling rights trackers
extern int white_castle_k;
extern int white_castle_q;
extern int black_castle_k;
extern int black_castle_q;

// En Passant Target Square (-1 if unavailable)
extern int ep_target_r;
extern int ep_target_c;

typedef struct {
    int src_r, src_c;
    int dst_r, dst_c;
    char promo; // '\0' if no promotion, otherwise 'Q','R','B','N' or 'q','r','b','n'
} Move;

typedef struct {
    Move moves[1024];
    int count;
} MoveList;

void init_board();
void print_board();
void execute_move(Move m);
Move parse_move(const char* move_str);
void move_to_string(Move m, char* buffer);
// UCI-style format: "e2e4", "e7e8q" (no dash, lowercase promotion letter)
void move_to_uci_string(Move m, char* buffer);

char execute_move_with_capture(Move m);
void undo_move(Move m, char captured_piece);
void store_board();
int check_threefold_repetition();

#endif // CHESS_H