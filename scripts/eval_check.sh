#!/usr/bin/env bash
# Eval symmetry: every position and its colour mirror must score equal and opposite.
cd "$(dirname "$0")/.."
make -s || exit 1
fail=0
while read -r fen; do
  read -r a b <<<"$(./bin/segmate --eval "$fen")"
  if [ -n "$a" ] && [ "$a" -eq $((-b)) ]; then echo "PASS: $a / $b  $fen"
  else echo "FAIL: '$a' / '$b'  $fen"; fail=1; fi
done <<'FENS'
rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1
r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1
8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - - 0 1
r3k2r/Pppp1ppp/1b3nbN/nP6/BBP1P3/q4N2/Pp1P2PP/R2Q1RK1 w kq - 0 1
rnbq1k1r/pp1Pbppp/2p5/8/2B5/8/PPP1NnPP/RNBQK2R w KQ - 1 8
r4rk1/1pp1qppp/p1np1n2/2b1p1B1/2B1P1b1/P1NP1N2/1PP1QPPP/R4RK1 w - - 0 10
8/5k2/8/8/8/R4K2/8/8 w - - 0 1
6k1/5ppp/8/8/8/8/1P6/K7 w - - 0 1
FENS
[ "$fail" -eq 0 ] && echo "Eval checks passed." || { echo "Eval failures."; exit 1; }
