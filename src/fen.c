// fen.c - Forsyth-Edwards Notation parsing and generation
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "chess.h"
#include "fen.h"
#include "platform.h"

// Parses a FEN string and fully resets game state from it.
// Accepts the standard 6-field FEN:
//   <placement> <stm> <castling> <ep> <halfmove> <fullmove>
int set_board_from_fen(const char* fen) {
    if (fen == NULL) return -1;

    // Copy so we can tokenize without modifying the caller's string.
    char buf[256];
    strncpy(buf, fen, sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = '\0';

    char* saveptr = NULL;
    char* placement = strtok_r(buf, " ", &saveptr);
    char* stm       = strtok_r(NULL, " ", &saveptr);
    char* castling  = strtok_r(NULL, " ", &saveptr);
    char* ep        = strtok_r(NULL, " ", &saveptr);
    char* halfmove  = strtok_r(NULL, " ", &saveptr);
    char* fullmove  = strtok_r(NULL, " ", &saveptr);

    if (placement == NULL || stm == NULL || castling == NULL || ep == NULL) {
        return -1; // First four fields are mandatory.
    }

    // --- 1. Piece placement: 8 ranks separated by '/', rank 8 first ---
    // Reset board to empty first.
    for (int r = 0; r < 8; r++)
        for (int c = 0; c < 8; c++)
            board[r][c] = '.';

    int rank = 0;
    int file = 0;
    for (int i = 0; placement[i] != '\0'; i++) {
        char ch = placement[i];
        if (ch == '/') {
            rank++;
            file = 0;
            if (rank >= 8) return -1;
        } else if (ch >= '1' && ch <= '8') {
            file += ch - '0';
            if (file > 8) return -1;
        } else if (strchr("pnbrqkPNBRQK", ch) != NULL) {
            if (file >= 8) return -1;
            board[rank][file] = ch;
            file++;
        } else {
            return -1; // Unexpected character.
        }
    }
    if (rank != 7 || file != 8) return -1; // Must fill exactly 8 ranks of 8.

    // --- 2. Side to move ---
    if (stm[0] == 'w') {
        side_to_move = 1;
    } else if (stm[0] == 'b') {
        side_to_move = 0;
    } else {
        return -1;
    }

    // --- 3. Castling availability ---
    white_castle_k = 0; white_castle_q = 0;
    black_castle_k = 0; black_castle_q = 0;
    if (castling[0] == '-') {
        // No castling rights; nothing to set.
    } else {
        for (int i = 0; castling[i] != '\0'; i++) {
            switch (castling[i]) {
                case 'K': white_castle_k = 1; break;
                case 'Q': white_castle_q = 1; break;
                case 'k': black_castle_k = 1; break;
                case 'q': black_castle_q = 1; break;
                default: return -1;
            }
        }
    }

    // --- 4. En passant target square (algebraic) or '-' ---
    ep_target_r = -1;
    ep_target_c = -1;
    if (ep[0] != '-') {
        if (ep[0] < 'a' || ep[0] > 'h') return -1;
        if (ep[1] < '1' || ep[1] > '8') return -1;
        ep_target_c = ep[0] - 'a';
        ep_target_r = 8 - (ep[1] - '0');
    }

    // --- 5. Halfmove clock (50-move rule). Optional in some FENs. ---
    if (halfmove != NULL) {
        halfmove_clock = atoi(halfmove);
        if (halfmove_clock < 0) halfmove_clock = 0;
    } else {
        halfmove_clock = 0;
    }

    // --- 6. Fullmove number. Optional in some FENs. ---
    if (fullmove != NULL) {
        fullmove_number = atoi(fullmove);
        if (fullmove_number < 1) fullmove_number = 1;
    } else {
        fullmove_number = 1;
    }

    // Reset repetition history. A position loaded from FEN has occurred once:
    // earlier positions in the actual game are unknown and must not count.
    move_count = 0;
    reset_zobrist();
    store_board();

    return 0;
}

// Generates the FEN string for the current position into `out`.
void board_to_fen(char* out) {
    int pos = 0;

    // 1. Piece placement, rank 8 first.
    for (int r = 0; r < 8; r++) {
        int empty = 0;
        for (int c = 0; c < 8; c++) {
            char p = board[r][c];
            if (p == '.') {
                empty++;
            } else {
                if (empty > 0) {
                    out[pos++] = '0' + empty;
                    empty = 0;
                }
                out[pos++] = p;
            }
        }
        if (empty > 0) out[pos++] = '0' + empty;
        if (r < 7) out[pos++] = '/';
    }

    // 2. Side to move.
    out[pos++] = ' ';
    out[pos++] = side_to_move ? 'w' : 'b';

    // 3. Castling availability.
    out[pos++] = ' ';
    int any_castle = 0;
    if (white_castle_k) { out[pos++] = 'K'; any_castle = 1; }
    if (white_castle_q) { out[pos++] = 'Q'; any_castle = 1; }
    if (black_castle_k) { out[pos++] = 'k'; any_castle = 1; }
    if (black_castle_q) { out[pos++] = 'q'; any_castle = 1; }
    if (!any_castle) out[pos++] = '-';

    // 4. En passant target square.
    out[pos++] = ' ';
    if (ep_target_r == -1) {
        out[pos++] = '-';
    } else {
        out[pos++] = 'a' + ep_target_c;
        out[pos++] = '1' + (7 - ep_target_r);
    }

    // 5. Halfmove clock and 6. fullmove number.
    pos += sprintf(out + pos, " %d %d", halfmove_clock, fullmove_number);
}
