// search.c - Move selection: basic MCTS (UCB1, random playouts)
//
// The board is global state and execute_move() also mutates castling rights,
// en passant, clocks and repetition history (undo_move only fixes squares),
// so every simulation starts by restoring a snapshot of the root position.
#include <math.h>
#include <stdlib.h>
#include "search.h"
#include "validation.h"

int mcts_iterations = 2000;

#define UCB_C 1.41
#define PLAYOUT_MAX_PLIES 150

typedef struct {
    Move move;          // move that led to this node
    int parent, first_child, next_sibling, n_children;
    int mover_white;    // who played `move`
    int visits;
    double wins;        // from the mover's point of view (draw = 0.5)
} Node;

// White's score (1 / 0.5 / 0) if the position is over, else -1.
// ponytail: threefold repetition ignored inside the search (O(history) per call); add if draws matter.
static double terminal_score(const MoveList* l) {
    if (l->count == 0) {
        if (!is_in_check(side_to_move)) return 0.5;
        return side_to_move ? 0.0 : 1.0;
    }
    return halfmove_clock >= 100 ? 0.5 : -1;
}

static double playout(void) {
    for (int ply = 0; ply < PLAYOUT_MAX_PLIES; ply++) {
        MoveList l;
        get_legal_moves(side_to_move, &l);
        double t = terminal_score(&l);
        if (t >= 0) return t;
        execute_move(l.moves[rand() % l.count]);
    }
    return 0.5;
}

static int select_child(const Node* nodes, int n) {
    double lnN = log(nodes[n].visits);
    int best = -1;
    double best_v = -1;
    for (int c = nodes[n].first_child; c != -1; c = nodes[c].next_sibling) {
        double v = nodes[c].wins / nodes[c].visits + UCB_C * sqrt(lnN / nodes[c].visits);
        if (v > best_v) { best_v = v; best = c; }
    }
    return best;
}

Move find_best_move(void) {
    Move none = {-1, -1, -1, -1, '\0'};
    MoveList root_moves;
    get_legal_moves(side_to_move, &root_moves);
    if (root_moves.count == 0) return none;
    if (root_moves.count == 1) return root_moves.moves[0];

    int iters = mcts_iterations;
    Node* nodes = malloc(sizeof(Node) * (iters + 1));
    if (!nodes) return root_moves.moves[0];
    int used = 1;
    nodes[0] = (Node){none, -1, -1, -1, 0, 0, 0, 0};

    GameState root;
    save_state(&root);

    for (int i = 0; i < iters; i++) {
        restore_state(&root);
        int n = 0;
        for (;;) {
            MoveList l;
            get_legal_moves(side_to_move, &l);
            if (terminal_score(&l) >= 0) break;
            if (nodes[n].n_children < l.count) { // expand next untried move
                Move m = l.moves[nodes[n].n_children++];
                int c = used++;
                nodes[c] = (Node){m, n, -1, nodes[n].first_child, 0, side_to_move, 0, 0};
                nodes[n].first_child = c;
                execute_move(m);
                n = c;
                break;
            }
            n = select_child(nodes, n);
            execute_move(nodes[n].move);
        }
        double white_score = playout();
        for (; n != -1; n = nodes[n].parent) {
            nodes[n].visits++;
            nodes[n].wins += nodes[n].mover_white ? white_score : 1 - white_score;
        }
    }

    restore_state(&root);
    int best = -1;
    for (int c = nodes[0].first_child; c != -1; c = nodes[c].next_sibling)
        if (best == -1 || nodes[c].visits > nodes[best].visits) best = c;
    Move result = nodes[best].move;
    free(nodes);
    return result;
}
