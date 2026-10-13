// chess.c - Core chess logic implementation
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "chess.h"

char board[8][8];
#define HISTORY_MAX 2048
static uint64_t history[HISTORY_MAX]; // zobrist hash after each stored position
int move_count = 0;
int halfmove_clock = 0;
int fullmove_number = 1;
int side_to_move = 1; // 1 = White to move

int white_castle_k = 1; int white_castle_q = 1;
int black_castle_k = 1; int black_castle_q = 1;

int ep_target_r = -1;
int ep_target_c = -1;

uint64_t zobrist;
static uint64_t zobrist_ep; // the en passant key currently in `zobrist` (0 = none)

static const uint64_t KEYS[781] = {
#include "zobrist_keys.h"
};

static uint64_t piece_key(char p, int r, int c) {
    static const char kinds[] = "pPnNbBrRqQkK";
    int kind = (int)(strchr(kinds, p) - kinds);
    return KEYS[64 * kind + 8 * (7 - r) + c];
}

static uint64_t castle_key(void) {
    return (white_castle_k ? KEYS[768] : 0) ^ (white_castle_q ? KEYS[769] : 0) ^
           (black_castle_k ? KEYS[770] : 0) ^ (black_castle_q ? KEYS[771] : 0);
}

// The en passant file counts only if a pawn of the side to move stands ready to capture.
static uint64_t ep_key(void) {
    if (ep_target_r == -1) return 0;
    int r = ep_target_r + (side_to_move ? 1 : -1);
    char pawn = side_to_move ? 'P' : 'p';
    if ((ep_target_c > 0 && board[r][ep_target_c - 1] == pawn) ||
        (ep_target_c < 7 && board[r][ep_target_c + 1] == pawn))
        return KEYS[772 + ep_target_c];
    return 0;
}

uint64_t compute_zobrist(void) {
    uint64_t h = castle_key() ^ ep_key() ^ (side_to_move ? KEYS[780] : 0);
    for (int r = 0; r < 8; r++)
        for (int c = 0; c < 8; c++)
            if (board[r][c] != '.') h ^= piece_key(board[r][c], r, c);
    return h;
}

void reset_zobrist(void) {
    zobrist = compute_zobrist();
    zobrist_ep = ep_key();
}

// Records the current position's hash for repetition checks.
void store_board() {
    if (move_count >= HISTORY_MAX) return;
    history[move_count++] = zobrist;
}

// Checks if the current position has occurred 3 or more times (only positions
// since the last capture or pawn move can repeat).
int check_threefold_repetition() {
    int repetitions = 0;
    for (int i = move_count - 1; i >= 0 && i >= move_count - 1 - halfmove_clock; i--)
        if (history[i] == zobrist) repetitions++;
    return repetitions >= 3;
}

int is_repetition(void) {
    for (int i = move_count - 3; i >= 0 && i >= move_count - 1 - halfmove_clock; i -= 2)
        if (history[i] == zobrist) return 1;
    return 0;
}

void save_state(GameState* s) {
    memcpy(s->board, board, sizeof(board));
    s->stm = side_to_move;
    s->wk = white_castle_k; s->wq = white_castle_q;
    s->bk = black_castle_k; s->bq = black_castle_q;
    s->epr = ep_target_r; s->epc = ep_target_c;
    s->halfmove = halfmove_clock; s->fullmove = fullmove_number;
    s->move_count = move_count; // history entries below this are never modified
    s->hash = zobrist; s->ep_key = zobrist_ep;
}

void restore_state(const GameState* s) {
    memcpy(board, s->board, sizeof(board));
    side_to_move = s->stm;
    white_castle_k = s->wk; white_castle_q = s->wq;
    black_castle_k = s->bk; black_castle_q = s->bq;
    ep_target_r = s->epr; ep_target_c = s->epc;
    halfmove_clock = s->halfmove; fullmove_number = s->fullmove;
    move_count = s->move_count;
    zobrist = s->hash; zobrist_ep = s->ep_key;
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
    fullmove_number = 1;
    side_to_move = 1; // White to move

    white_castle_k = 1; white_castle_q = 1;
    black_castle_k = 1; black_castle_q = 1;
    ep_target_r = -1; ep_target_c = -1;

    reset_zobrist();
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
    char moving = board[m.src_r][m.src_c];
    
    // Check if this is an En Passant capture
    if ((moving == 'P' || moving == 'p') && m.dst_r == ep_target_r && m.dst_c == ep_target_c) {
        cap = board[m.src_r][m.dst_c]; // The captured pawn is horizontally adjacent
        board[m.src_r][m.dst_c] = '.'; // Remove the enemy pawn
    }
    
    if (m.promo != '\0') {
        board[m.dst_r][m.dst_c] = m.promo; // Place the promoted piece
    } else {
        board[m.dst_r][m.dst_c] = moving;
    }
    board[m.src_r][m.src_c] = '.';

    // Physically move the Rook if this is a Castling move
    if ((moving == 'K' || moving == 'k') && abs(m.src_c - m.dst_c) == 2) {
        if (m.dst_c == 6) { // Kingside
            board[m.src_r][5] = board[m.src_r][7];
            board[m.src_r][7] = '.';
        } else if (m.dst_c == 2) { // Queenside
            board[m.src_r][3] = board[m.src_r][0];
            board[m.src_r][0] = '.';
        }
    }

    return cap;
}

//undoes a move - for move legality
void undo_move(Move m, char captured_piece) {
    char moving = board[m.dst_r][m.dst_c];
    if (m.promo != '\0') {
        moving = (m.dst_r == 0) ? 'P' : 'p'; // Revert promoted piece back to a pawn
    }
    board[m.src_r][m.src_c] = moving;
    
    // Undo En Passant specific logic
    if ((moving == 'P' || moving == 'p') && m.dst_r == ep_target_r && m.dst_c == ep_target_c) {
        board[m.dst_r][m.dst_c] = '.'; // The destination square was actually empty
        board[m.src_r][m.dst_c] = captured_piece; // Restore enemy pawn beside us
    } else {
        board[m.dst_r][m.dst_c] = captured_piece; // Normal capture restore
    }

    // Undo the Rook movement if it was a Castling move
    if ((moving == 'K' || moving == 'k') && abs(m.src_c - m.dst_c) == 2) {
        if (m.dst_c == 6) { // Kingside
            board[m.src_r][7] = board[m.src_r][5];
            board[m.src_r][5] = '.';
        } else if (m.dst_c == 2) { // Queenside
            board[m.src_r][0] = board[m.src_r][3];
            board[m.src_r][3] = '.';
        }
    }
}

// actually makes move
void execute_move(Move m) {
    char moving_piece = board[m.src_r][m.src_c];

    // Zobrist: take out every square this move can touch (and the castling /
    // en passant / side keys), then put them back in after the move.
    int sq[4][2] = {{m.src_r, m.src_c}, {m.dst_r, m.dst_c}}, n_sq = 2;
    if (m.src_r != m.dst_r && m.src_c != m.dst_c) { // diagonal: maybe an en passant victim (harmless if untouched)
        sq[n_sq][0] = m.src_r; sq[n_sq++][1] = m.dst_c;
    }
    if ((moving_piece | 32) == 'k' && abs(m.src_c - m.dst_c) == 2) { // castling rook
        sq[n_sq][0] = m.src_r; sq[n_sq++][1] = m.dst_c == 6 ? 7 : 0;
        sq[n_sq][0] = m.src_r; sq[n_sq++][1] = m.dst_c == 6 ? 5 : 3;
    }
    uint64_t h = zobrist ^ castle_key() ^ zobrist_ep ^ (side_to_move ? KEYS[780] : 0);
    for (int i = 0; i < n_sq; i++)
        if (board[sq[i][0]][sq[i][1]] != '.') h ^= piece_key(board[sq[i][0]][sq[i][1]], sq[i][0], sq[i][1]);
    
    // Revoke castling rights if King or Rooks move
    if (moving_piece == 'K') { white_castle_k = 0; white_castle_q = 0; }
    if (moving_piece == 'k') { black_castle_k = 0; black_castle_q = 0; }
    if (moving_piece == 'R') {
        if (m.src_r == 7 && m.src_c == 0) white_castle_q = 0;
        if (m.src_r == 7 && m.src_c == 7) white_castle_k = 0;
    }
    if (moving_piece == 'r') {
        if (m.src_r == 0 && m.src_c == 0) black_castle_q = 0;
        if (m.src_r == 0 && m.src_c == 7) black_castle_k = 0;
    }
    
    // Revoke castling rights if a Rook is captured in its corner
    if (m.dst_r == 7 && m.dst_c == 0) white_castle_q = 0;
    if (m.dst_r == 7 && m.dst_c == 7) white_castle_k = 0;
    if (m.dst_r == 0 && m.dst_c == 0) black_castle_q = 0;
    if (m.dst_r == 0 && m.dst_c == 7) black_castle_k = 0;

    char captured = execute_move_with_capture(m);
    
    // 50 move rule draw
    if (moving_piece == 'P' || moving_piece == 'p' || captured != '.') {
        halfmove_clock = 0;
    } else {
        halfmove_clock++;
    }

    // Update En Passant Target state for nextturn
    ep_target_r = -1;
    ep_target_c = -1;
    if (moving_piece == 'P' && m.src_r - m.dst_r == 2) {
        ep_target_r = m.src_r - 1; // Square behind the white pawn
        ep_target_c = m.src_c;
    } else if (moving_piece == 'p' && m.dst_r - m.src_r == 2) {
        ep_target_r = m.src_r + 1; // Square behind the black pawn
        ep_target_c = m.src_c;
    }

    // Advance side to move and (after Black) the fullmove number
    if (!side_to_move) fullmove_number++;
    side_to_move = !side_to_move;

    for (int i = 0; i < n_sq; i++)
        if (board[sq[i][0]][sq[i][1]] != '.') h ^= piece_key(board[sq[i][0]][sq[i][1]], sq[i][0], sq[i][1]);
    zobrist_ep = ep_key();
    zobrist = h ^ castle_key() ^ zobrist_ep ^ (side_to_move ? KEYS[780] : 0);

    store_board();
}

// Parses moves in format "e2e4", "e2-e4", or "e7e8q"
Move parse_move(const char* move_str) {
    Move m = {-1, -1, -1, -1, '\0'};
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
                state = 2; // Coordinates loaded
            }
        } else if (state == 2 && (ch == 'q' || ch == 'r' || ch == 'b' || ch == 'n' || ch == 'Q' || ch == 'R' || ch == 'B' || ch == 'N')) {
            // Assign matching case based on destination rank
            if (ch >= 'a' && ch <= 'z') {
                m.promo = (m.dst_r == 0) ? (ch - 32) : ch;
            } else {
                m.promo = (m.dst_r == 7) ? (ch + 32) : ch;
            }
        }
    }
    return m;
}

// str function for Move struct basically
void move_to_string(Move m, char* buffer) {
    if (m.promo != '\0') {
        char p = m.promo;
        if (p >= 'A' && p <= 'Z') p += 32; // Lowercase formatting for output
        sprintf(buffer, "%c%d-%c%d%c",
            m.src_c + 'a', 8 - m.src_r,
            m.dst_c + 'a', 8 - m.dst_r, p);
    } else {
        sprintf(buffer, "%c%d-%c%d",
            m.src_c + 'a', 8 - m.src_r,
            m.dst_c + 'a', 8 - m.dst_r);
    }
}

// UCI move format: "e2e4", "e7e8q" (no dash, lowercase promotion letter).
// Used when communicating with chess GUIs.
void move_to_uci_string(Move m, char* buffer) {
    if (m.promo != '\0') {
        char p = m.promo;
        if (p >= 'A' && p <= 'Z') p += 32; // UCI promotion letters are lowercase
        sprintf(buffer, "%c%d%c%d%c",
            m.src_c + 'a', 8 - m.src_r,
            m.dst_c + 'a', 8 - m.dst_r, p);
    } else {
        sprintf(buffer, "%c%d%c%d",
            m.src_c + 'a', 8 - m.src_r,
            m.dst_c + 'a', 8 - m.dst_r);
    }
}