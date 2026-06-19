#!/usr/bin/env bash
# Verifies the engine only ever returns legal first moves from the start
# position, and that they're among the 20 known legal opening moves.
set -e
cd /mnt/c/Users/Elect/Desktop/SegMate

legal=(a2a3 a2a4 b2b3 b2b4 c2c3 c2c4 d2d3 d2d4 e2e4 e2e3 f2f3 f2f4 g2g3 g2g4 h2h3 h2h4 b1a3 b1c3 g1f3 g1h3)

bad=0
for i in $(seq 1 30); do
  mv=$(printf 'position startpos\ngo\nquit\n' | ./bin/segmate --uci | grep '^bestmove' | awk '{print $2}')
  ok=0
  for l in "${legal[@]}"; do [ "$mv" = "$l" ] && ok=1 && break; done
  if [ "$ok" -ne 1 ]; then echo "ILLEGAL first move: $mv"; bad=1; fi
done
[ "$bad" -eq 0 ] && echo "All 30 sampled startpos moves were legal."
