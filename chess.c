// chess.c - Core chess logic implementation
#include <stdio.h>
#include "chess.h"

char board[8][8];
char history[512][8][8]; 
int move_count = 0;
int halfmove_clock = 0;
// Stores the current board state in the history array for threefold repetition checking.
void store_board() {
    if (move_count >= 512) return; 
    for (int r = 0; r < 8; r++) {
        for (int c = 0; c < 8; c++) {
            history[move_count][r][c] = board[r][c];
        }
    }
    move_count++;
}
// Checks if the current position has occurred 3 or more times in the history.
int check_threefold_repetition() {
    int repetitions = 0;
    for (int i = 0; i < move_count; i++) {
        int match = 1;
        for (int r = 0; r < 8; r++) {
            for (int c = 0; c < 8; c++) {
                if (history[i][r][c] != board[r][c]) {
                    match = 0;
                    break;
                }
            }
            if (!match) break;
        }
        if (match) repetitions++;
    }
    return repetitions >= 3;
}
// initialises board
void init_board() {
    char starting_board[8][8] = {
        {'r', 'n', 'b', 'q', 'k', 'b', 'n', 'r'},
        {'p', 'p', 'p', 'p', 'p', 'p', 'p', 'p'},
        {'.', '.', '.', '.', '.', '.', '.', '.'},
        {'.', '.', '.', '.', '.', '.', '.', '.'},
        {'.', '.', '.', '.', '.', '.', '.', '.'},
        {'.', '.', '.', '.', '.', '.', '.', '.'},
        {'P', 'P', 'P', 'P', 'P', 'P', 'P', 'P'},
        {'R', 'N', 'B', 'Q', 'K', 'B', 'N', 'R'}
    };
    for (int r = 0; r < 8; r++) {
        for (int c = 0; c < 8; c++) {
            board[r][c] = starting_board[r][c];
        }
    }
    move_count = 0;
    halfmove_clock = 0;
    store_board(); 
}
// prints board
void print_board() {
    printf("\n");
    for (int r = 0; r < 8; r++) {
        printf("%d | ", 8 - r); 
        for (int c = 0; c < 8; c++) {
            printf("%c ", board[r][c]);
        }
        printf("\n");
    }
    printf("  -----------------\n");
    printf("    a b c d e f g h\n\n");
}
// Makes moves and returns captured piece (or '.' if none)
char execute_move_with_capture(Move m) {
    char cap = board[m.dst_r][m.dst_c];
    board[m.dst_r][m.dst_c] = board[m.src_r][m.src_c];
    board[m.src_r][m.src_c] = '.';
    return cap;
}
//undoes a move - for move legality
void undo_move(Move m, char captured_piece) {
    board[m.src_r][m.src_c] = board[m.dst_r][m.dst_c];
    board[m.dst_r][m.dst_c] = captured_piece;
}
// actually makes move
void execute_move(Move m) {
    char moving_piece = board[m.src_r][m.src_c];
    char captured = execute_move_with_capture(m);
    
    // 50 move rule draw
    if (moving_piece == 'P' || moving_piece == 'p' || captured != '.') {
        halfmove_clock = 0;
    } else {
        halfmove_clock++;
    }

    store_board(); 
}
// Parses moves in format "e2e4" or "e2-e4"
Move parse_move(const char* move_str) {
    Move m = {-1, -1, -1, -1};
    int state = 0; 
    for (int i = 0; move_str[i] != '\0'; i++) {
        char ch = move_str[i];
        if (ch >= 'a' && ch <= 'h') {
            if (state == 0) m.src_c = ch - 'a';
            else m.dst_c = ch - 'a';
        } else if (ch >= '1' && ch <= '8') {
            if (state == 0) {
                m.src_r = 8 - (ch - '0'); 
                state = 1;              
            } else {
                m.dst_r = 8 - (ch - '0');
            }
        }
    }
    return m;
}
// str function for Move struct basically
void move_to_string(Move m, char* buffer) {
    sprintf(buffer, "%c%d-%c%d", 
        m.src_c + 'a', 8 - m.src_r, 
        m.dst_c + 'a', 8 - m.dst_r);
}