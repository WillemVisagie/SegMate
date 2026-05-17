// validation.c - Move generation and legality checking
#include <stdio.h>
#include <ctype.h>
#include "chess.h"
#include "validation.h"

int is_white_piece(char p) { return p >= 'A' && p <= 'Z'; }
int is_black_piece(char p) { return p >= 'a' && p <= 'z'; }
// Checks if a piece belongs to the enemy
int is_enemy(char p, int is_white) {
    if (p == '.') return 0; 
    return is_white ? is_black_piece(p) : is_white_piece(p);
}
// Checks if a piece belongs to the current player
int is_own(char p, int is_white) {
    if (p == '.') return 0;
    return is_white ? is_white_piece(p) : is_black_piece(p);
}
// add move to a MoveList
void add_move(MoveList* list, int sr, int sc, int dr, int dc) {
    if (dr >= 0 && dr < 8 && dc >= 0 && dc < 8) {
        list->moves[list->count++] = (Move){sr, sc, dr, dc};
    }
}

// Generates moves assuming the King is perfectly safe (Ignores checks)
void get_pseudo_legal_moves(int is_white, MoveList* move_list) {
    move_list->count = 0;
    int dr[] = {-1, 1, 0, 0, -1, -1, 1, 1};
    int dc[] = {0, 0, -1, 1, -1, 1, -1, 1};

    for (int r = 0; r < 8; r++) {
        for (int c = 0; c < 8; c++) {
            char piece = board[r][c];
            if (piece == '.') continue;
            if (is_white && !is_white_piece(piece)) continue;
            if (!is_white && !is_black_piece(piece)) continue;

            if (piece == 'P') { 
                if (r - 1 >= 0 && board[r - 1][c] == '.') {
                    add_move(move_list, r, c, r - 1, c);
                    if (r == 6 && board[r - 2][c] == '.') add_move(move_list, r, c, r - 2, c); 
                }
                if (r - 1 >= 0 && c - 1 >= 0 && is_black_piece(board[r - 1][c - 1])) add_move(move_list, r, c, r - 1, c - 1);
                if (r - 1 >= 0 && c + 1 < 8 && is_black_piece(board[r - 1][c + 1])) add_move(move_list, r, c, r - 1, c + 1);
            } 
            else if (piece == 'p') { 
                if (r + 1 < 8 && board[r + 1][c] == '.') {
                    add_move(move_list, r, c, r + 1, c);
                    if (r == 1 && board[r + 2][c] == '.') add_move(move_list, r, c, r + 2, c); 
                }
                if (r + 1 < 8 && c - 1 >= 0 && is_white_piece(board[r + 1][c - 1])) add_move(move_list, r, c, r + 1, c - 1);
                if (r + 1 < 8 && c + 1 < 8 && is_white_piece(board[r + 1][c + 1])) add_move(move_list, r, c, r + 1, c + 1);
            } 
            else if (piece == 'N' || piece == 'n') { 
                int kr[] = {-2, -2, -1, -1, 1, 1, 2, 2};
                int kc[] = {-1, 1, -2, 2, -2, 2, -1, 1};
                for(int i = 0; i < 8; i++) {
                    int nr = r + kr[i], nc = c + kc[i];
                    if (nr >= 0 && nr < 8 && nc >= 0 && nc < 8 && !is_own(board[nr][nc], is_white)) {
                        add_move(move_list, r, c, nr, nc);
                    }
                }
            } 
            else { 
                int start_dir = (piece == 'B' || piece == 'b') ? 4 : 0;
                int end_dir = (piece == 'R' || piece == 'r') ? 4 : 8;
                int is_king = (piece == 'K' || piece == 'k');

                for (int i = start_dir; i < end_dir; i++) {
                    int nr = r, nc = c;
                    while (1) {
                        nr += dr[i]; nc += dc[i];
                        if (nr < 0 || nr >= 8 || nc < 0 || nc >= 8) break; 
                        if (is_own(board[nr][nc], is_white)) break; 
                        
                        add_move(move_list, r, c, nr, nc);
                        
                        if (is_enemy(board[nr][nc], is_white)) break; 
                        if (is_king) break; 
                    }
                }
            }
        }
    }
}
// Checks if current player is in check after a move
int is_in_check(int is_white) {
    int kr = -1, kc = -1;
    char king_char = is_white ? 'K' : 'k';
    
    for (int r = 0; r < 8; r++) {
        for (int c = 0; c < 8; c++) {
            if (board[r][c] == king_char) { kr = r; kc = c; break; }
        }
        if (kr != -1) break;
    }
    if (kr == -1) return 0; 

    MoveList enemy_moves;
    get_pseudo_legal_moves(!is_white, &enemy_moves);
    for (int i = 0; i < enemy_moves.count; i++) {
        if (enemy_moves.moves[i].dst_r == kr && enemy_moves.moves[i].dst_c == kc) {
            return 1;
        }
    }
    return 0;
}

// Filters pseudo-legal moves. Only keeps moves where your King survives.
void get_legal_moves(int is_white, MoveList* move_list) {
    MoveList pseudo;
    get_pseudo_legal_moves(is_white, &pseudo);
    move_list->count = 0;

    for (int i = 0; i < pseudo.count; i++) {
        Move m = pseudo.moves[i];
        
        char captured = execute_move_with_capture(m);
        
        if (!is_in_check(is_white)) {
            move_list->moves[move_list->count++] = m;
        }
        
        undo_move(m, captured);
    }
}
// checks if a move is legal
int check_if_legal(Move m, int is_white) {
    MoveList list;
    get_legal_moves(is_white, &list);
    for (int i = 0; i < list.count; i++) {
        if (list.moves[i].src_r == m.src_r && list.moves[i].src_c == m.src_c &&
            list.moves[i].dst_r == m.dst_r && list.moves[i].dst_c == m.dst_c) {
            return 1; 
        }
    }
    return 0; 
}
// Checks if game is over and returns result
// -1 -> Black wins
// 0 -> Draw
// 1 -> White wins
// 2 -> Game not over
// 3 -> Draw by 50-move rule
int check_if_over(int is_white_turn) {
    if (halfmove_clock >= 100) return 3; // Draw by 50-move rule
    if (check_threefold_repetition()) return 0; // Draw by repetition

    MoveList list;
    get_legal_moves(is_white_turn, &list);
    
    if (list.count == 0) {
        if (is_in_check(is_white_turn)) {
            return is_white_turn ? -1 : 1; 
        } else {
            return 0; // Stalemate
        }
    }
    return 2; // Not over
}