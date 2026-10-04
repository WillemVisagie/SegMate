// search.c - Move selection: basic MCTS (UCB1, random playouts)
//
// The board is global state and execute_move() also mutates castling rights,
// en passant, clocks and repetition history (undo_move only fixes squares),
// so every simulation starts by restoring a snapshot of the root position.
#include <math.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "book.h"
#include "search.h"
#include "validation.h"

int mcts_iterations = 2000;

static atomic_int stop_flag;
void search_clear_stop(void) { atomic_store(&stop_flag, 0); }
void search_request_stop(void) { atomic_store(&stop_flag, 1); }
int search_stop_requested(void) { return atomic_load(&stop_flag); }

static double now_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec * 1000.0 + ts.tv_nsec / 1e6;
}

#define UCB_C 1.41
#define MAX_NODES (1 << 20) // pool cap for time/infinite searches (~60MB virtual)
#define INFO_INTERVAL_MS 1000
#define PV_MAX 8
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

// Prints a UCI info line: PV = most-visited path, score from the best child's win rate.
static void print_info(const Node* nodes, long iterations, double elapsed_ms) {
    int best = -1;
    for (int c = nodes[0].first_child; c != -1; c = nodes[c].next_sibling)
        if (best == -1 || nodes[c].visits > nodes[best].visits) best = c;
    if (best == -1) return;

    double p = nodes[best].wins / nodes[best].visits;
    p = p < 0.001 ? 0.001 : p > 0.999 ? 0.999 : p;
    int cp = (int)(400 * log10(p / (1 - p)));

    char pv[PV_MAX * 6 + 1] = "";
    int depth = 0;
    for (int n = best; n != -1 && depth < PV_MAX; depth++) {
        char buf[8];
        move_to_uci_string(nodes[n].move, buf);
        strcat(pv, depth ? " " : "");
        strcat(pv, buf);
        int next = -1;
        for (int c = nodes[n].first_child; c != -1; c = nodes[c].next_sibling)
            if (next == -1 || nodes[c].visits > nodes[next].visits) next = c;
        n = next;
    }
    long ms = (long)elapsed_ms;
    printf("info depth %d nodes %ld score cp %d time %ld nps %ld pv %s\n",
           depth, iterations, cp, ms, ms > 0 ? iterations * 1000 / ms : 0, pv);
}

Move find_best_move(void) {
    SearchLimits none = {0, 0, 0, 0};
    return search_best_move(&none);
}

Move search_best_move(const SearchLimits* lim) {
    Move none = {-1, -1, -1, -1, '\0'};
    MoveList root_moves;
    get_legal_moves(side_to_move, &root_moves);
    if (root_moves.count == 0) return none;
    if (root_moves.count == 1) return root_moves.moves[0];

    Move book_move;
    if (book_probe(&book_move)) return book_move;

    // Iteration cap: explicit nodes limit, else a big pool when time/stop bounds
    // the search, else the fixed default.
    int bounded_by_time = lim->time_ms > 0 || lim->infinite;
    long cap = lim->iterations > 0 ? lim->iterations
             : bounded_by_time ? MAX_NODES - 1 : mcts_iterations;
    if (cap > MAX_NODES - 1) cap = MAX_NODES - 1;

    Node* nodes = malloc(sizeof(Node) * (cap + 1));
    if (!nodes) return root_moves.moves[0];
    int used = 1;
    nodes[0] = (Node){none, -1, -1, -1, 0, 0, 0, 0};

    GameState root;
    save_state(&root);

    double start = now_ms(), last_info = start;
    long i;
    for (i = 0; i < cap; i++) {
        if (i > 0 && (i & 15) == 0) { // poll the clock/stop flag every 16 iterations
            double t = now_ms();
            if (search_stop_requested() || (lim->time_ms > 0 && t - start >= lim->time_ms)) break;
            if (lim->info && t - last_info >= INFO_INTERVAL_MS) {
                print_info(nodes, i, t - start);
                last_info = t;
            }
        }
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
    if (lim->info) print_info(nodes, i, now_ms() - start);
    int best = -1;
    for (int c = nodes[0].first_child; c != -1; c = nodes[c].next_sibling)
        if (best == -1 || nodes[c].visits > nodes[best].visits) best = c;
    Move result = best == -1 ? root_moves.moves[0] : nodes[best].move;
    free(nodes);
    return result;
}
