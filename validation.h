#ifndef VALIDATION_H
#define VALIDATION_H

#include "chess.h"

// Exposes checking logic
int is_in_check(int is_white);
void get_legal_moves(int is_white, MoveList* move_list);
int check_if_legal(Move m, int is_white);

// Returns: 1 = White wins, -1 = Black wins, 0 = Draw, 2 = Game not over
int check_if_over(int is_white_turn); 

#endif // VALIDATION_H