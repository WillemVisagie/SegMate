// eval.c - Static evaluation: material, piece-square tables (middlegame and
// endgame king, blended by phase), pawn structure, king shelter, bishop pair.
// Tables are the "Simplified Evaluation Function" (chessprogramming.org),
// laid out like board[][]: index 0 = a8, White's view; Black reads them mirrored.
#include "chess.h"
#include <stdlib.h>
#include "eval.h"

static const int VALUE[6] = {100, 320, 330, 500, 900, 0}; // P N B R Q K

static const signed char PST[7][64] = {
    { // pawn
      0,  0,  0,  0,  0,  0,  0,  0,  50, 50, 50, 50, 50, 50, 50, 50,
     10, 10, 20, 30, 30, 20, 10, 10,   5,  5, 10, 25, 25, 10,  5,  5,
      0,  0,  0, 20, 20,  0,  0,  0,   5, -5,-10,  0,  0,-10, -5,  5,
      5, 10, 10,-20,-20, 10, 10,  5,   0,  0,  0,  0,  0,  0,  0,  0 },
    { // knight
    -50,-40,-30,-30,-30,-30,-40,-50, -40,-20,  0,  0,  0,  0,-20,-40,
    -30,  0, 10, 15, 15, 10,  0,-30, -30,  5, 15, 20, 20, 15,  5,-30,
    -30,  0, 15, 20, 20, 15,  0,-30, -30,  5, 10, 15, 15, 10,  5,-30,
    -40,-20,  0,  5,  5,  0,-20,-40, -50,-40,-30,-30,-30,-30,-40,-50 },
    { // bishop
    -20,-10,-10,-10,-10,-10,-10,-20, -10,  0,  0,  0,  0,  0,  0,-10,
    -10,  0,  5, 10, 10,  5,  0,-10, -10,  5,  5, 10, 10,  5,  5,-10,
    -10,  0, 10, 10, 10, 10,  0,-10, -10, 10, 10, 10, 10, 10, 10,-10,
    -10,  5,  0,  0,  0,  0,  5,-10, -20,-10,-10,-10,-10,-10,-10,-20 },
    { // rook
      0,  0,  0,  0,  0,  0,  0,  0,   5, 10, 10, 10, 10, 10, 10,  5,
     -5,  0,  0,  0,  0,  0,  0, -5,  -5,  0,  0,  0,  0,  0,  0, -5,
     -5,  0,  0,  0,  0,  0,  0, -5,  -5,  0,  0,  0,  0,  0,  0, -5,
     -5,  0,  0,  0,  0,  0,  0, -5,   0,  0,  0,  5,  5,  0,  0,  0 },
    { // queen
    -20,-10,-10, -5, -5,-10,-10,-20, -10,  0,  0,  0,  0,  0,  0,-10,
    -10,  0,  5,  5,  5,  5,  0,-10,  -5,  0,  5,  5,  5,  5,  0, -5,
      0,  0,  5,  5,  5,  5,  0, -5, -10,  5,  5,  5,  5,  5,  0,-10,
    -10,  0,  5,  0,  0,  0,  0,-10, -20,-10,-10, -5, -5,-10,-10,-20 },
    { // king, middlegame
    -30,-40,-40,-50,-50,-40,-40,-30, -30,-40,-40,-50,-50,-40,-40,-30,
    -30,-40,-40,-50,-50,-40,-40,-30, -30,-40,-40,-50,-50,-40,-40,-30,
    -20,-30,-30,-40,-40,-30,-30,-20, -10,-20,-20,-20,-20,-20,-20,-10,
     20, 20,  0,  0,  0,  0, 20, 20,  20, 30, 10,  0,  0, 10, 30, 20 },
    { // king, endgame
    -50,-40,-30,-20,-20,-30,-40,-50, -30,-20,-10,  0,  0,-10,-20,-30,
    -30,-10, 20, 30, 30, 20,-10,-30, -30,-10, 30, 40, 40, 30,-10,-30,
    -30,-10, 30, 40, 40, 30,-10,-30, -30,-10, 20, 30, 30, 20,-10,-30,
    -30,-30,  0,  0,  0,  0,-30,-30, -50,-30,-30,-30,-30,-30,-30,-50 },
};

static int kind(char p) {
    switch (p | 32) {
        case 'p': return 0; case 'n': return 1; case 'b': return 2;
        case 'r': return 3; case 'q': return 4; default: return 5;
    }
}

int evaluate(void) {
    int mg = 0, eg = 0, phase = 0;           // White minus Black
    int pawns[2][8] = {{0}}, bishops[2] = {0}; // [white][file]
    int kr[2] = {0}, kc[2] = {0};
    for (int r = 0; r < 8; r++)
        for (int c = 0; c < 8; c++) {
            char p = board[r][c];
            if (p == '.') continue;
            int w = p < 'a', k = kind(p), sign = w ? 1 : -1;
            int sq = w ? r * 8 + c : (7 - r) * 8 + c;
            int v = VALUE[k] + PST[k][sq];
            if (k == 5) {
                mg += sign * PST[5][sq];
                eg += sign * PST[6][sq];
                kr[w] = r; kc[w] = c;
                continue;
            }
            mg += sign * v;
            eg += sign * v;
            phase += (int[]){0, 1, 1, 2, 4}[k];
            if (k == 0) pawns[w][c]++;
            if (k == 2) bishops[w]++;
        }

    for (int w = 0; w < 2; w++) {
        int sign = w ? 1 : -1, s = 0, dir = w ? -1 : 1; // dir: pawn's forward row step
        if (bishops[w] >= 2) s += 30;
        for (int c = 0; c < 8; c++) {
            if (!pawns[w][c]) continue;
            if (pawns[w][c] > 1) s -= 15 * (pawns[w][c] - 1); // doubled
            if ((c == 0 || !pawns[w][c - 1]) && (c == 7 || !pawns[w][c + 1])) s -= 15 * pawns[w][c]; // isolated
        }
        // Passed pawns: no enemy pawn ahead on this or an adjacent file.
        char own = w ? 'P' : 'p', enemy = w ? 'p' : 'P';
        for (int r = 1; r < 7; r++)
            for (int c = 0; c < 8; c++) {
                if (board[r][c] != own) continue;
                int passed = 1;
                for (int rr = r + dir; rr >= 0 && rr < 8 && passed; rr += dir)
                    for (int cc = c - 1; cc <= c + 1; cc++)
                        if (cc >= 0 && cc < 8 && board[rr][cc] == enemy) passed = 0;
                if (passed) {
                    int adv = w ? 6 - r : r - 1; // 0 on the start rank .. 5 on the 7th
                    eg += sign * (10 + 15 * adv);
                    mg += sign * (5 + 5 * adv);
                }
            }
        // King shelter (middlegame): own pawns on the three files in front of a castled-side king.
        if (kc[w] <= 2 || kc[w] >= 5) {
            int missing = 0;
            for (int c = kc[w] - 1; c <= kc[w] + 1; c++) {
                if (c < 0 || c > 7) continue;
                int r1 = kr[w] + dir, r2 = kr[w] + 2 * dir;
                int ok = (r1 >= 0 && r1 < 8 && board[r1][c] == own) || (r2 >= 0 && r2 < 8 && board[r2][c] == own);
                if (!ok) missing++;
            }
            mg -= sign * 15 * missing;
        }
        mg += sign * s;
        eg += sign * s;
    }

    // Mop-up: when clearly ahead, drive the lone king to the edge and walk ours up to it.
    if (eg >= 400 || eg <= -400) {
        int w = eg > 0, l = !w; // winning / losing side
        int edge = (kr[l] < 4 ? 3 - kr[l] : kr[l] - 4) + (kc[l] < 4 ? 3 - kc[l] : kc[l] - 4);
        int dist = abs(kr[w] - kr[l]) + abs(kc[w] - kc[l]);
        eg += (w ? 1 : -1) * (10 * edge + 4 * (14 - dist));
    }

    if (phase > 24) phase = 24;
    return (mg * phase + eg * (24 - phase)) / 24;
}
