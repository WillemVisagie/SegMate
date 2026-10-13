// search.c - Move selection: MCTS (UCB1, capture-biased playouts with a material cutoff)
//
// The board is global state and execute_move() also mutates castling rights,
// en passant, clocks and repetition history (undo_move only fixes squares),
// so every simulation starts by restoring a snapshot of the root position.
#include <math.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "book.h"
#include "eval.h"
#include "platform.h"
#include "search.h"
#include "validation.h"

int mcts_iterations = 2000;
int swindle_pct = 40;
int beauty_pct = 50;
int smart_playouts = 2;
int playout_plies = 4;

static atomic_int stop_flag;
void search_clear_stop(void) { atomic_store(&stop_flag, 0); }
void search_request_stop(void) { atomic_store(&stop_flag, 1); }
int search_stop_requested(void) { return atomic_load(&stop_flag); }

#define UCB_C 1.41
#define MAX_NODES (1 << 20) // pool cap for time/infinite searches (~60MB virtual)
#define INFO_INTERVAL_MS 1000
#define PV_MAX 8
#define PLAYOUT_MAX_PLIES 150
#define CUTOFF_PAWNS 5 // smart playouts: material lead that counts as a win

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

static int material_balance(int white);

// No pawns, rooks or queens and at most one minor piece: nobody can mate.
static int insufficient_material(void) {
    int minors = 0;
    for (int i = 0; i < 64; i++)
        switch (board[i / 8][i % 8] | 32) {
            case 'p': case 'r': case 'q': return 0;
            case 'n': case 'b': if (++minors > 1) return 0; break;
        }
    return 1;
}

// White's expected score from a material lead in pawns / an eval in centipawns,
// always strictly between a mate for either side.
static double material_score(int bal) { return 0.02 + 0.96 / (1 + pow(10, -bal / 4.0)); }
static double eval_score(void) { return 0.02 + 0.96 / (1 + pow(10, -evaluate() / 400.0)); }

// Plays semi-random moves and returns White's score. *mated = 1 if it ended in
// checkmate. Level 0: uniform random to the end (or the ply cap). Level 1+:
// captures/promotions preferred, stop once material swings by CUTOFF_PAWNS from
// where the playout started (relative, so an already-won position still gets
// real playouts toward mate). Level 1 scores an unfinished game by material,
// level 2 stops after playout_plies and scores by evaluate().
static double playout(int* mated) {
    *mated = 0;
    int start_bal = material_balance(1);
    int max_plies = smart_playouts >= 2 ? playout_plies : PLAYOUT_MAX_PLIES;
    for (int ply = 0; ply < max_plies; ply++) {
        if (smart_playouts && (ply & 3) == 0) {
            int bal = material_balance(1);
            if (abs(bal - start_bal) >= CUTOFF_PAWNS) return smart_playouts >= 2 ? eval_score() : material_score(bal);
            if (insufficient_material()) return 0.5;
        }
        Move m;
        if (!random_legal_move(side_to_move, &m, smart_playouts)) {
            if (!is_in_check(side_to_move)) return 0.5;
            *mated = 1;
            return side_to_move ? 0.0 : 1.0;
        }
        if (halfmove_clock >= 100) return 0.5;
        execute_move(m);
    }
    return smart_playouts >= 2 ? eval_score() : smart_playouts ? material_score(material_balance(1)) : 0.5;
}

// --- Swindler / Aesthete helpers -------------------------------------------

static int piece_value(char p) {
    switch (p | 32) { // lowercase
        case 'p': return 1;
        case 'n': case 'b': return 3;
        case 'r': return 5;
        case 'q': return 9;
        default: return 0;
    }
}

// Material (pawns) from `white`'s point of view.
static int material_balance(int white) {
    int bal = 0;
    for (int r = 0; r < 8; r++)
        for (int c = 0; c < 8; c++) {
            char p = board[r][c];
            if (p == '.') continue;
            int v = piece_value(p);
            bal += ((p >= 'A' && p <= 'Z') == white) ? v : -v;
        }
    return bal;
}

// Root side's non-pawn, non-king pieces on the board.
static int minor_major_count(int white) {
    int n = 0;
    for (int r = 0; r < 8; r++)
        for (int c = 0; c < 8; c++) {
            char p = board[r][c];
            if (p != '.' && (p >= 'A' && p <= 'Z') == white && piece_value(p) > 1) n++;
        }
    return n;
}

// Per-iteration record of what happened along the tree path.
typedef struct {
    int start_bal, min_bal; // root side's material at the root / its lowest along the path
    int underpromo;         // root side under-promoted
} PathTrace;

// Call right after a tree move was played by `mover`.
static void trace_move(PathTrace* t, Move m, int mover, int root_white) {
    int bal = material_balance(root_white);
    if (bal < t->min_bal) t->min_bal = bal;
    if (mover == root_white && m.promo != '\0' && (m.promo | 32) != 'q') t->underpromo = 1;
}

// How beautiful the iteration was for the root side, 0..1.
static double beauty(const PathTrace* t, double root_score, int mated, int root_white) {
    double b = 0;
    if (root_score >= 0.5 && t->underpromo) b = 0.5;
    if (root_score == 1.0) {
        int sac = t->start_bal - t->min_bal; // material given up that then paid off
        if (sac >= 2) b = fmax(b, (sac > 6 ? 6 : sac) / 6.0);
        if (mated) {
            int n = minor_major_count(root_white);
            b = fmax(b, n <= 1 ? 1.0 : n <= 2 ? 0.6 : 0.2); // fewer pieces = prettier mate
        }
    }
    return b;
}

// Opponent temperature: cooler when we're winning (play solid), hotter when
// losing (assume more mistakes are available and go for traps).
static double swindle_temperature(double root_value) {
    double base = swindle_pct / 100.0;
    double mult = root_value >= 0.65 ? 0.4 : root_value <= 0.35 ? 2.0
                : 2.0 - (root_value - 0.35) / 0.30 * 1.6;
    return base * mult;
}

// Expected value of root child `c` for us if the opponent picks its reply by
// softmax over the replies' values (for them) at temperature `temp`.
static double swindle_value(const Node* nodes, int c, double temp) {
    double vmax = -1e9, total = 0, ev = 0;
    for (int r = nodes[c].first_child; r != -1; r = nodes[r].next_sibling)
        if (nodes[r].wins / nodes[r].visits > vmax) vmax = nodes[r].wins / nodes[r].visits;
    if (vmax == -1e9) return nodes[c].wins / nodes[c].visits;
    for (int r = nodes[c].first_child; r != -1; r = nodes[r].next_sibling) {
        double v = nodes[r].wins / nodes[r].visits, w = exp((v - vmax) / temp);
        total += w;
        ev += w * (1 - v);
    }
    return ev / total;
}

// Among well-explored root moves, the one with the best value against the fallible opponent.
static int swindle_pick(const Node* nodes, int max_visits, double temp) {
    int best = -1;
    double best_v = -1;
    for (int c = nodes[0].first_child; c != -1; c = nodes[c].next_sibling) {
        if (nodes[c].visits * 4 < max_visits) continue;
        double v = swindle_value(nodes, c, temp);
        if (v > best_v) { best_v = v; best = c; }
    }
    return best;
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

// Prints a UCI info line: PV = `best` (-1 = most visited root move) then the
// most-visited path, score from that move's win rate.
static void print_info(const Node* nodes, int best, long iterations, double elapsed_ms) {
    if (best == -1) for (int c = nodes[0].first_child; c != -1; c = nodes[c].next_sibling)
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

// --- Tree reuse -------------------------------------------------------------
// The tree of the last search is kept, one per root colour (so in --match each
// side keeps its own). The next search looks for the grandchild matching the
// new position (our move + their reply) and continues from its subtree.
static Node* kept[2];
static GameState kept_root[2];
static long reuse_searches, reuse_hits;

static int same_position(const GameState* a, const GameState* b) {
    return memcmp(a->board, b->board, sizeof(a->board)) == 0 && a->stm == b->stm &&
           a->wk == b->wk && a->wq == b->wq && a->bk == b->bk && a->bq == b->bq &&
           a->epr == b->epr && a->epc == b->epc;
}

// Kept-tree node (depth 2) whose position equals `now`, or -1. Leaves the board at `now`.
static int find_reusable(int side, const GameState* now) {
    const Node* t = kept[side];
    int found = -1;
    if (!t) return -1;
    for (int c = t[0].first_child; c != -1 && found == -1; c = t[c].next_sibling)
        for (int g = t[c].first_child; g != -1 && found == -1; g = t[g].next_sibling) {
            restore_state(&kept_root[side]);
            move_count = now->move_count; // replay above the real history, never over it
            execute_move(t[c].move);
            execute_move(t[g].move);
            GameState s;
            save_state(&s);
            if (same_position(&s, now)) found = g;
        }
    restore_state(now);
    return found;
}

static int subtree_size(const Node* t, int n) {
    int size = 1;
    for (int c = t[n].first_child; c != -1; c = t[c].next_sibling) size += subtree_size(t, c);
    return size;
}

// Copies the subtree at `n` into dst (preorder, sibling order kept) and returns its index.
static int copy_subtree(const Node* src, int n, Node* dst, int* used, int parent) {
    int d = (*used)++;
    dst[d] = src[n];
    dst[d].parent = parent;
    dst[d].first_child = dst[d].next_sibling = -1;
    int prev = -1;
    for (int c = src[n].first_child; c != -1; c = src[c].next_sibling) {
        int dc = copy_subtree(src, c, dst, used, d);
        if (prev == -1) dst[d].first_child = dc;
        else dst[prev].next_sibling = dc;
        prev = dc;
    }
    return d;
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

    GameState root;
    save_state(&root);
    int root_white = side_to_move;

    int reuse = find_reusable(root_white, &root), reused = 0;
    reuse_searches++;
    if (reuse != -1) {
        reused = subtree_size(kept[root_white], reuse);
        if (reused > MAX_NODES / 2) reuse = -1, reused = 0;
    }
    long pool = cap + reused + 1;
    if (pool > MAX_NODES) pool = MAX_NODES;
    Node* nodes = malloc(sizeof(Node) * pool);
    if (!nodes) return root_moves.moves[0];
    int used = 0;
    if (reuse != -1) {
        copy_subtree(kept[root_white], reuse, nodes, &used, -1);
        reuse_hits++;
    } else {
        nodes[used++] = (Node){none, -1, -1, -1, 0, 0, 0, 0};
    }
    free(kept[root_white]);
    kept[root_white] = NULL;
    if (cap > pool - used) cap = pool - used;

    int start_bal = material_balance(root_white);
    double beauty_w = beauty_pct / 1000.0, beauty_sum = 0;

    double start = plat_now_ms(), last_info = start;
    long i;
    for (i = 0; i < cap; i++) {
        if (i > 0 && (i & 15) == 0) { // poll the clock/stop flag every 16 iterations
            double t = plat_now_ms();
            if (search_stop_requested() || (lim->time_ms > 0 && t - start >= lim->time_ms)) break;
            if (lim->info && t - last_info >= INFO_INTERVAL_MS) {
                print_info(nodes, -1, i, t - start);
                last_info = t;
            }
        }
        restore_state(&root);
        int n = 0;
        PathTrace trace = {start_bal, start_bal, 0};
        for (;;) {
            MoveList l;
            get_legal_moves(side_to_move, &l);
            if (terminal_score(&l) >= 0) break;
            int mover = side_to_move;
            if (nodes[n].n_children < l.count) { // expand next untried move
                Move m = l.moves[nodes[n].n_children++];
                int c = used++;
                nodes[c] = (Node){m, n, -1, nodes[n].first_child, 0, mover, 0, 0};
                nodes[n].first_child = c;
                execute_move(m);
                trace_move(&trace, m, mover, root_white);
                n = c;
                break;
            }
            n = select_child(nodes, n);
            execute_move(nodes[n].move);
            trace_move(&trace, nodes[n].move, mover, root_white);
        }
        int mated;
        double white_score = playout(&mated);
        if (beauty_w > 0) { // Aesthete: small bonus on top of the result, from the root side's view
            double root_score = root_white ? white_score : 1 - white_score;
            double bonus = beauty_w * beauty(&trace, root_score, mated, root_white);
            beauty_sum += bonus;
            white_score += root_white ? bonus : -bonus;
        }
        for (; n != -1; n = nodes[n].parent) {
            nodes[n].visits++;
            nodes[n].wins += nodes[n].mover_white ? white_score : 1 - white_score;
        }
    }

    restore_state(&root);
    int best = -1;
    for (int c = nodes[0].first_child; c != -1; c = nodes[c].next_sibling)
        if (best == -1 || nodes[c].visits > nodes[best].visits) best = c;
    double temp = 0;
    if (swindle_pct > 0 && best != -1) { // Swindler: re-pick against a fallible opponent
        temp = swindle_temperature(nodes[best].wins / nodes[best].visits);
        best = swindle_pick(nodes, nodes[best].visits, temp);
    }
    if (lim->info) {
        print_info(nodes, best, i, plat_now_ms() - start);
        if (swindle_pct > 0 || beauty_w > 0)
            printf("info string swindle temp %.2f, avg beauty bonus %.4f\n", temp, i ? beauty_sum / i : 0.0);
        printf("info string tree reuse: kept %d of the previous tree, hits %ld/%ld\n", reused, reuse_hits, reuse_searches);
    }
    Move result = best == -1 ? root_moves.moves[0] : nodes[best].move;
    Node* shrunk = realloc(nodes, sizeof(Node) * used); // keep the tree for the next search
    kept[root_white] = shrunk ? shrunk : nodes;
    kept_root[root_white] = root;
    return result;
}
