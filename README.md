# SegMate

A chess engine written in C. It has a fully working move generator (all rules
including castling, en passant, promotion, check/checkmate/stalemate, the
50-move rule, and threefold repetition) and speaks the
[UCI protocol](http://wbec-ridderkerk.nl/html/UCIProtocol.html), so it can be
driven by any standard chess GUI.

The engine's brain is currently a **random mover** — it picks a uniformly
random legal move. The move-selection logic is isolated behind a single seam
(`find_best_move()` in `src/search.c`) so a real search can be dropped in later
without touching the protocol or move-generation code.

## Requirements

- A C compiler (`gcc`/`clang`) and `make`.

## Building

```bash
make            # builds bin/segmate
make run        # builds and runs the interactive referee
make run-uci    # builds and launches in UCI mode
make clean      # removes bin/
```

## Running

### Interactive referee (default)

```bash
./bin/segmate
```

Launches a menu for Human-vs-Human, Human-vs-AI, AI-vs-Human, and AI-vs-AI
play. (This is the default only when stdin is a terminal; use `--referee` to
force it when input is piped.) Moves are entered in coordinate form, e.g.
`e2e4`, `e2-e4`, or `e7e8q` (promotion suffix). Type `quit` or `resign` during
a game.

### UCI mode

The engine auto-detects how it's being driven, so a chess GUI (Cute Chess,
Arena, pychess, lichess-bot, …) can simply point at the plain executable with
no arguments: when stdin is piped (not a terminal) it speaks UCI; when stdin
is a terminal it launches the interactive referee. You can also force either
mode explicitly:

```bash
./bin/segmate --uci       # or -u  — always speak UCI on stdin
./bin/segmate --referee   # or -r  — always launch the referee menu
./bin/segmate             # auto-detect (UCI if piped, referee if interactive)
```

In UCI mode the engine reads commands from stdin and writes responses to
stdout.

A manual session:

```bash
$ ./bin/segmate --uci
uci
id name SegMate
id author Willem Visagie
option name Random Move type check default true
uciok
isready
readyok
position startpos moves e2e4 e7e5 g1f3
go movetime 1000
bestmove f8d6        # random — will differ each run until a real search lands
quit
```

#### Supported commands

| Command | Behaviour |
| --- | --- |
| `uci` | Identify the engine and reply `uciok`. |
| `isready` | Reply `readyok`. |
| `ucinewgame` | Reset to the starting position. |
| `setoption` | Accepted/silently ignored (no configurable options yet). |
| `position startpos \| fen <fen> [moves <m1> <m2> …]` | Set up the board, then apply the move list. |
| `go [wtime btime … movetime depth nodes]` | Search and reply `bestmove <uci>`. Time/depth fields are accepted but ignored while the mover is random. |
| `stop` / `ponderhit` | Accepted (no asynchronous search yet). |
| `quit` | Exit. |

When the side to move has no legal move the engine replies `bestmove (none)`.

Move format is standard UCI: `e2e4`, `e7e8q` (lowercase promotion letter, no
dash).

## Testing

```bash
bash scripts/verify_all.sh   # full build + FEN round-trip + UCI checks (run in WSL/Linux)
bash scripts/uci_smoke.sh    # asserts sampled startpos replies are all legal
```

`verify_all.sh` exercises a clean `-Wall -Wextra` build, FEN round-trips on
five positions, the UCI handshake, and castling / en passant / promotion /
stalemate through the UCI move path.

## Project layout

```
src/
  chess.c/.h      Board model, make/undo, move parsing/formatting, FEN state
  validation.c/.h Move generation, legality, check & game-over detection
  referee.c/.h    Interactive Human/AI game loop
  fen.c/.h        FEN parsing and generation
  search.c/.h     Move selection (currently the random mover) — the engine seam
  uci.c/.h        UCI protocol loop
  randommove.c/.h Random legal-move picker
  main.c          Launch-mode dispatch (auto-detects UCI vs referee)
scripts/
  verify_all.sh   Full post-build verification
  uci_smoke.sh    Startpos move-legality sampler
```

## Notes & roadmap

- The search is intentionally a placeholder. A negamax + alpha-beta engine with
  evaluation, iterative deepening, and a transposition table belongs in
  `src/search.c`; UCI threading and time management plug into the same seam.
- Repetition history is reset when a position is loaded from FEN (a loaded
  position counts once; earlier game history is unknown), matching UCI
  semantics.
```
