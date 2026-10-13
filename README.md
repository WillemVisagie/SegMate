# SegMate

A chess engine in C with full rules, a UCI interface, and an MCTS brain
(UCB1 selection, short capture-biased playouts scored by a static evaluation)
with a personality: part **Swindler**, part
**Aesthete**. A random move selector is included
baseline opponent.

## Build & run

```bash
make                    # build bin/segmate (needs gcc/clang + make)
./bin/segmate           # referee menu in a terminal, UCI when stdin is piped
./bin/segmate --uci     # force UCI (for chess GUIs)
./bin/segmate --referee # force the referee menu
```

## Personality

- **Swindler** (`Swindle`, 0-100, default 0 = off): only when the position
  looks worse, it may pick a slightly lower-rated move if the opponent's
  replies to it are worse on average, i.e. one where only a few replies hold.
  The bound is `Swindle/500` of win probability, scaled by how bad it looks.
  Against MCTS opponents it has cost strength in every form tried, so it is
  opt-in.
- **Aesthete** (`Beauty`, 0-100, default 50; 0 = off): a small bonus for
  sacrifices that pay off, under-promotions that don't lose, and mates with few
  pieces. It never outweighs a win.

Both are UCI options. Regardless of them, the final move is picked by a reply
check: among well-explored moves, the best against (about) the opponent's best
reply, a 2-ply minimax over the tree's statistics. It scored ~79% against the
same engine picking the most-visited move.

## MCTS vs random

```bash
scripts/mock_match.sh                  # 2 games, watch every move
./bin/segmate --match N [--watch]      # N games vs random, alternating colours
./bin/segmate --match N --weak 200 --iters 200 --swindle 0 --beauty 0 --nobook
                                       # vs plain MCTS; A/B the personality settings
scripts/ab.sh 400 --weak 1000 --iters 1000 --nobook   # same, in parallel on all cores, with score +- SE
```

## Tests

```bash
scripts/verify_all.sh   # build, FEN round-trips, UCI checks
scripts/uci_smoke.sh    # startpos bestmove legality
scripts/perft.sh        # move generator vs known perft counts
scripts/time_check.sh   # go movetime/wtime/nodes/infinite + stop
scripts/book_check.sh   # opening book
scripts/eval_check.sh   # eval symmetry (position vs colour mirror)
```

## Files

- `src/chess.c`, `validation.c`, `fen.c` — board, move generation/legality, FEN
- `src/search.c` — MCTS (`find_best_move()` is the engine seam)
- `src/eval.c` — static evaluation (material, piece-square tables, pawns, king shelter, mop-up)
- `src/book.c` — opening book (data from the CC0 Lichess chess-openings set, keyed by Polyglot zobrist hash)
- `src/randommove.c` — random mover baseline
- `src/uci.c`, `referee.c`, `match.c` — UCI loop, interactive game, MCTS-vs-random
- `src/main.c` — mode dispatch

## Portability

`src/platform.h` is a small shim (threads, sleep, clock, `getpid`, `isatty`)
so the engine builds on Linux, macOS and Windows (MinGW/MSYS2) without a
rewrite. Search runs on a worker thread so `stop` works; all OS-specific code
lives in that one header. The Windows branch is written but untested.
