// perft.c - Move-generator correctness check (leaf-node counts at fixed depth)
#include <stdio.h>
#include "chess.h"
#include "perft.h"
#include "validation.h"

unsigned long long perft(int depth) {
    if (depth == 0) return 1;
    MoveList l;
    get_legal_moves(side_to_move, &l);
    if (depth == 1) return l.count;

    GameState s;
    save_state(&s);
    unsigned long long nodes = 0;
    for (int i = 0; i < l.count; i++) {
        execute_move(l.moves[i]);
        nodes += perft(depth - 1);
        restore_state(&s);
    }
    return nodes;
}

unsigned long long perft_divide(int depth) {
    MoveList l;
    get_legal_moves(side_to_move, &l);

    GameState s;
    save_state(&s);
    unsigned long long total = 0;
    for (int i = 0; i < l.count; i++) {
        char buf[16];
        move_to_uci_string(l.moves[i], buf);
        execute_move(l.moves[i]);
        unsigned long long n = depth > 1 ? perft(depth - 1) : 1;
        restore_state(&s);
        printf("%s: %llu\n", buf, n);
        total += n;
    }
    printf("\nNodes: %llu\n", total);
    return total;
}
