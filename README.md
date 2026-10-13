# SegMate

A chess engine in C with full rules, a UCI interface, and an MCTS brain
(UCB1 selection, random playouts) with a personality: part **Swindler**, part
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

- **Swindler** (`Swindle`, 0-100, default 40; 0 = off): after the search, it
  re-picks among the well-explored moves assuming a *fallible* opponent who
  chooses replies by softmax over their value instead of always the best one,
  so it prefers moves where only a few replies hold. It runs cooler when
  winning and hotter when losing.
- **Aesthete** (`Beauty`, 0-100, default 50; 0 = off): a small bonus for
  sacrifices that pay off, under-promotions that don't lose, and mates with few
  pieces. It never outweighs a win.

Both are UCI options. With both at 0 it is plain MCTS.

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
```

## Files

- `src/chess.c`, `validation.c`, `fen.c` — board, move generation/legality, FEN
- `src/search.c` — MCTS (`find_best_move()` is the engine seam)
- `src/book.c` — opening book (data from the CC0 Lichess chess-openings set)
- `src/randommove.c` — random mover baseline
- `src/uci.c`, `referee.c`, `match.c` — UCI loop, interactive game, MCTS-vs-random
- `src/main.c` — mode dispatch

## Portability

`src/platform.h` is a small shim (threads, sleep, clock, `getpid`, `isatty`)
so the engine builds on Linux, macOS and Windows (MinGW/MSYS2) without a
rewrite. Search runs on a worker thread so `stop` works; all OS-specific code
lives in that one header. The Windows branch is written but untested.
