// validation.c - Move generation and legality checking
#include <stdio.h>
#include <ctype.h>
#include "chess.h"
#include "validation.h"
#include <stdlib.h>

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
    if (list->count >= 1024) return; // Boundary check against stack buffer overflow
    if (dr >= 0 && dr < 8 && dc >= 0 && dc < 8) {
        list->moves[list->count++] = (Move){sr, sc, dr, dc, '\0'};
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
                // Forward single push
                if (r - 1 >= 0 && board[r - 1][c] == '.') {
                    if (r - 1 == 0) { // Promotion!
                        char promos[] = {'Q', 'R', 'B', 'N'};
                        for (int p = 0; p < 4; p++) {
                            if (move_list->count < 1024) {
                                move_list->moves[move_list->count++] = (Move){r, c, r - 1, c, promos[p]};
                            }
                        }
                    } else {
                        add_move(move_list, r, c, r - 1, c);
                        if (r == 6 && board[r - 2][c] == '.') add_move(move_list, r, c, r - 2, c); 
                    }
                }
                // Capture Left
                if (r - 1 >= 0 && c - 1 >= 0 && is_black_piece(board[r - 1][c - 1])) {
                    if (r - 1 == 0) { // Promotion!
                        char promos[] = {'Q', 'R', 'B', 'N'};
                        for (int p = 0; p < 4; p++) {
                            if (move_list->count < 1024) {
                                move_list->moves[move_list->count++] = (Move){r, c, r - 1, c - 1, promos[p]};
                            }
                        }
                    } else {
                        add_move(move_list, r, c, r - 1, c - 1);
                    }
                }
                // Capture Right
                if (r - 1 >= 0 && c + 1 < 8 && is_black_piece(board[r - 1][c + 1])) {
                    if (r - 1 == 0) { // Promotion!
                        char promos[] = {'Q', 'R', 'B', 'N'};
                        for (int p = 0; p < 4; p++) {
                            if (move_list->count < 1024) {
                                move_list->moves[move_list->count++] = (Move){r, c, r - 1, c + 1, promos[p]};
                            }
                        }
                    } else {
                        add_move(move_list, r, c, r - 1, c + 1);
                    }
                }
                
                // En Passant for White
                if (ep_target_r == r - 1 && abs(ep_target_c - c) == 1) {
                    add_move(move_list, r, c, ep_target_r, ep_target_c);
                }
            } 
            else if (piece == 'p') { 
                // Forward single push
                if (r + 1 < 8 && board[r + 1][c] == '.') {
                    if (r + 1 == 7) { // Promotion!
                        char promos[] = {'q', 'r', 'b', 'n'};
                        for (int p = 0; p < 4; p++) {
                            if (move_list->count < 1024) {
                                move_list->moves[move_list->count++] = (Move){r, c, r + 1, c, promos[p]};
                            }
                        }
                    } else {
                        add_move(move_list, r, c, r + 1, c);
                        if (r == 1 && board[r + 2][c] == '.') add_move(move_list, r, c, r + 2, c); 
                    }
                }
                // Capture Left
                if (r + 1 < 8 && c - 1 >= 0 && is_white_piece(board[r + 1][c - 1])) {
                    if (r + 1 == 7) { // Promotion!
                        char promos[] = {'q', 'r', 'b', 'n'};
                        for (int p = 0; p < 4; p++) {
                            if (move_list->count < 1024) {
                                move_list->moves[move_list->count++] = (Move){r, c, r + 1, c - 1, promos[p]};
                            }
                        }
                    } else {
                        add_move(move_list, r, c, r + 1, c - 1);
                    }
                }
                // Capture Right
                if (r + 1 < 8 && c + 1 < 8 && is_white_piece(board[r + 1][c + 1])) {
                    if (r + 1 == 7) { // Promotion!
                        char promos[] = {'q', 'r', 'b', 'n'};
                        for (int p = 0; p < 4; p++) {
                            if (move_list->count < 1024) {
                                move_list->moves[move_list->count++] = (Move){r, c, r + 1, c + 1, promos[p]};
                            }
                        }
                    } else {
                        add_move(move_list, r, c, r + 1, c + 1);
                    }
                }
                
                // En Passant for Black
                if (ep_target_r == r + 1 && abs(ep_target_c - c) == 1) {
                    add_move(move_list, r, c, ep_target_r, ep_target_c);
                }
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

// What legality testing needs to know about the side to move, computed once
// per position: king square, check, and which own pieces are pinned to the king.
typedef struct {
    int kr, kc, in_check;
    unsigned long long pinned; // bit r*8+c
} KingInfo;

static void king_info(int is_white, KingInfo* k) {
    char king_char = is_white ? 'K' : 'k';
    k->kr = k->kc = -1;
    k->pinned = 0;
    for (int i = 0; i < 64 && k->kr == -1; i++)
        if (board[i / 8][i % 8] == king_char) { k->kr = i / 8; k->kc = i % 8; }
    if (k->kr == -1) { k->in_check = 0; return; }
    k->in_check = is_square_attacked(k->kr, k->kc, !is_white);

    // A pin: walking out from the king, one own piece then an enemy slider on that line.
    int dr[] = {-1, 1, 0, 0, -1, -1, 1, 1};
    int dc[] = {0, 0, -1, 1, -1, 1, -1, 1};
    for (int d = 0; d < 8; d++) {
        int r = k->kr, c = k->kc, own = -1;
        for (;;) {
            r += dr[d]; c += dc[d];
            if (r < 0 || r >= 8 || c < 0 || c >= 8) break;
            char p = board[r][c];
            if (p == '.') continue;
            if (is_own(p, is_white)) {
                if (own != -1) break;
                own = r * 8 + c;
                continue;
            }
            char up = p & ~32; // uppercase
            if (own != -1 && (up == 'Q' || (d < 4 ? up == 'R' : up == 'B')))
                k->pinned |= 1ULL << own;
            break;
        }
    }
}

// Only king moves, pinned pieces, en passant and moves out of check can expose
// the king; everything else is legal without making the move.
static int is_legal(Move m, int is_white, const KingInfo* k) {
    char piece = board[m.src_r][m.src_c];
    int is_king = (piece | 32) == 'k';
    int is_ep = (piece | 32) == 'p' && m.dst_r == ep_target_r && m.dst_c == ep_target_c;
    if (!k->in_check && !is_king && !is_ep && !(k->pinned >> (m.src_r * 8 + m.src_c) & 1)) return 1;
    if (k->kr == -1) return 1;
    char captured = execute_move_with_capture(m);
    int legal = is_king ? !is_square_attacked(m.dst_r, m.dst_c, !is_white)
                        : !is_square_attacked(k->kr, k->kc, !is_white);
    undo_move(m, captured);
    return legal;
}

// get all legal moves
void get_legal_moves(int is_white, MoveList* move_list) {
    MoveList pseudo;
    get_pseudo_legal_moves(is_white, &pseudo);
    KingInfo k;
    king_info(is_white, &k);
    move_list->count = 0;
    for (int i = 0; i < pseudo.count; i++)
        if (is_legal(pseudo.moves[i], is_white, &k))
            move_list->moves[move_list->count++] = pseudo.moves[i];
}

int random_legal_move(int is_white, Move* out) {
    MoveList l;
    get_pseudo_legal_moves(is_white, &l);
    KingInfo k;
    king_info(is_white, &k);
    while (l.count > 0) { // pick at random, drop illegal picks until one sticks
        int i = rand() % l.count;
        if (is_legal(l.moves[i], is_white, &k)) { *out = l.moves[i]; return 1; }
        l.moves[i] = l.moves[--l.count];
    }
    return 0;
}

// checks if a move is legal
int check_if_legal(Move m, int is_white) {
    MoveList list;
    get_legal_moves(is_white, &list);
    for (int i = 0; i < list.count; i++) {
        if (list.moves[i].src_r == m.src_r && list.moves[i].src_c == m.src_c &&
            list.moves[i].dst_r == m.dst_r && list.moves[i].dst_c == m.dst_c &&
            list.moves[i].promo == m.promo) { // Ensure promotion piece type matches
            return 1; 
        }
    }
    return 0; 
}

// Checks if game is over and returns result
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