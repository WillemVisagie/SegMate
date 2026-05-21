// referee.c - Main game loop and user interaction
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "chess.h"
#include "referee.h"
#include "validation.h"
#include "randommove.h"

typedef enum { HUMAN, AI } PlayerType;

// starts a match based on user input and handles the game loop
void start_match() {
    int choice;
    PlayerType white_player, black_player;
    char input[64];

    srand(time(NULL));

    printf("=== Chess Engine Referee ===\n");
    printf("1. Human vs Human\n2. Human (White) vs AI (Black)\n");
    printf("3. AI (White) vs Human (Black)\n4. AI vs AI\nChoice: ");
    
    if (fgets(input, sizeof(input), stdin) == NULL) return;
    choice = atoi(input);

    switch(choice) {
        case 1: white_player = HUMAN; black_player = HUMAN; break;
        case 2: white_player = HUMAN; black_player = AI; break;
        case 3: white_player = AI; black_player = HUMAN; break;
        case 4: white_player = AI; black_player = AI; break;
        default: white_player = HUMAN; black_player = HUMAN; break;
    }

    init_board();
    print_board();
    int is_white_turn = 1; 

    while (1) {
        // --- End Game Checks ---
        int game_status = check_if_over(is_white_turn);
        if (game_status != 2) { // game over
            if (game_status == 1) printf("\nCHECKMATE! White wins.\n");
            else if (game_status == -1) printf("\nCHECKMATE! Black wins.\n");
            else if (game_status == 3) printf("\nDRAW! (50-Move Rule Limit Reached).\n");
            else printf("\nDRAW! (Stalemate or Threefold Repetition).\n");
            break;
        }

        PlayerType current_player = is_white_turn ? white_player : black_player;
        
        if (is_in_check(is_white_turn)) {
            printf("\n*** %s is in CHECK! ***\n", is_white_turn ? "White" : "Black");
        } else {
            printf("\n%s's turn.\n", is_white_turn ? "White" : "Black");
        }

        // --- Move Execution ---
        if (current_player == HUMAN) {
            printf("Enter move (or 'resign'/'quit'): ");
            if (fgets(input, sizeof(input), stdin) == NULL) break;
            input[strcspn(input, "\n")] = '\0'; 

            if (strcmp(input, "quit") == 0) break;
            if (strcmp(input, "resign") == 0) {
                printf("\n%s resigns. %s wins!\n", is_white_turn ? "White" : "Black", is_white_turn ? "Black" : "White");
                break;
            }

            Move m = parse_move(input);
            
            // Interactive human promotion prompt if suffix was left out
            if (m.src_c != -1) {
                char piece = board[m.src_r][m.src_c];
                if (((piece == 'P' && m.dst_r == 0) || (piece == 'p' && m.dst_r == 7)) && m.promo == '\0') {
                    printf("Promote pawn to (q/r/b/n): ");
                    char promo_input[16];
                    if (fgets(promo_input, sizeof(promo_input), stdin) != NULL) {
                        char p_char = promo_input[0];
                        if (p_char >= 'A' && p_char <= 'Z') p_char += 32; // Standardize to lowercase
                        if (p_char == 'q' || p_char == 'r' || p_char == 'b' || p_char == 'n') {
                            m.promo = is_white_turn ? (p_char - 32) : p_char;
                        }
                    }
                }
            }

            if (m.src_c == -1 || !check_if_legal(m, is_white_turn)) {
                printf("Invalid or illegal move! Please try again.\n");
                continue; 
            }
            execute_move(m);
        } else {
            Move m = get_random_move(is_white_turn);
            char move_str[16];
            move_to_string(m, move_str);
            printf("[AI plays: %s]\n", move_str);
            execute_move(m);
        }

        print_board();
        is_white_turn = !is_white_turn; 
    }
}

int main() {
    start_match();
    return 0;
}