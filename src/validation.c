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

// checks if a square is attacked by the enemy - used for check detection and castling validation
int is_square_attacked(int r, int c, int attacker_is_white) {
    // 1. Check Enemy Knights
    int kr[] = {-2, -2, -1, -1, 1, 1, 2, 2};
    int kc[] = {-1, 1, -2, 2, -2, 2, -1, 1};
    for(int i = 0; i < 8; i++) {
        int nr = r + kr[i], nc = c + kc[i];
        if(nr >= 0 && nr < 8 && nc >= 0 && nc < 8) {
            char p = board[nr][nc];
            if(p != '.' && is_own(p, attacker_is_white) && (p == 'N' || p == 'n')) return 1;
        }
    }
    
    // 2. Check Enemy Pawns (They attack "backwards" relative to their movement direction)
    int pawn_dir = attacker_is_white ? 1 : -1; 
    int pr = r + pawn_dir;
    if (pr >= 0 && pr < 8) {
        if (c - 1 >= 0) {
            char p = board[pr][c-1];
            if (p != '.' && is_own(p, attacker_is_white) && (p == 'P' || p == 'p')) return 1;
        }
        if (c + 1 < 8) {
            char p = board[pr][c+1];
            if (p != '.' && is_own(p, attacker_is_white) && (p == 'P' || p == 'p')) return 1;
        }
    }
    
    // 3. Check Enemy King (For adjacency logic)
    int dr[] = {-1, 1, 0, 0, -1, -1, 1, 1}; // 0-3 Orthogonal, 4-7 Diagonal
    int dc[] = {0, 0, -1, 1, -1, 1, -1, 1};
    for(int i = 0; i < 8; i++) {
        int nr = r + dr[i], nc = c + dc[i];
        if(nr >= 0 && nr < 8 && nc >= 0 && nc < 8) {
            char p = board[nr][nc];
            if(p != '.' && is_own(p, attacker_is_white) && (p == 'K' || p == 'k')) return 1;
        }
    }
    
    // 4. Check Enemy Sliding Pieces (Rooks, Bishops, Queens)
    for(int i = 0; i < 8; i++) {
        int nr = r, nc = c;
        while(1) {
            nr += dr[i]; nc += dc[i];
            if(nr < 0 || nr >= 8 || nc < 0 || nc >= 8) break;
            char p = board[nr][nc];
            if(p != '.') {
                if(is_own(p, attacker_is_white)) {
                    char up = p;
                    if (up >= 'a' && up <= 'z') up -= 32; // Convert to uppercase for check
                    if(up == 'Q') return 1;
                    if(i < 4 && up == 'R') return 1;  // Orthogonal = Rook
                    if(i >= 4 && up == 'B') return 1; // Diagonal = Bishop
                }
                break; // Blocked by own or enemy piece
            }
        }
    }
    return 0;
}

// Checks if current player's King is in check
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

    return is_square_attacked(kr, kc, !is_white);
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

                // Castling Logic
                if (piece == 'K') { // White King
                    if (white_castle_k && board[7][5] == '.' && board[7][6] == '.') {
                        if (!is_square_attacked(7, 4, 0) && !is_square_attacked(7, 5, 0) && !is_square_attacked(7, 6, 0)) {
                            add_move(move_list, 7, 4, 7, 6);
                        }
                    }
                    if (white_castle_q && board[7][1] == '.' && board[7][2] == '.' && board[7][3] == '.') {
                        if (!is_square_attacked(7, 4, 0) && !is_square_attacked(7, 3, 0) && !is_square_attacked(7, 2, 0)) {
                            add_move(move_list, 7, 4, 7, 2);
                        }
                    }
                } 
                else if (piece == 'k') { // Black King
                    if (black_castle_k && board[0][5] == '.' && board[0][6] == '.') {
                        if (!is_square_attacked(0, 4, 1) && !is_square_attacked(0, 5, 1) && !is_square_attacked(0, 6, 1)) {
                            add_move(move_list, 0, 4, 0, 6);
                        }
                    }
                    if (black_castle_q && board[0][1] == '.' && board[0][2] == '.' && board[0][3] == '.') {
                        if (!is_square_attacked(0, 4, 1) && !is_square_attacked(0, 3, 1) && !is_square_attacked(0, 2, 1)) {
                            add_move(move_list, 0, 4, 0, 2);
                        }
                    }
                }
            }
        }
    }
}
// get all legal moves
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