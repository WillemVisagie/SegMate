#!/usr/bin/env bash
# Checks the move generator against published perft node counts
# (https://www.chessprogramming.org/Perft_Results). Pass a max depth as $1 to
# go deeper (default 3; startpos depth 5 takes a while).
cd "$(dirname "$0")/.."
make -s || exit 1
max=${1:-3}
fail=0

check() { # name fen expected_d1 expected_d2 ...
  local name=$1 fen=$2; shift 2
  local d=1
  for exp in "$@"; do
    [ "$d" -gt "$max" ] && break
    got=$(./bin/segmate --perft "$d" "$fen" | awk '/^Nodes:/ {print $2}')
    if [ "$got" = "$exp" ]; then echo "PASS: $name depth $d = $got"
    else echo "FAIL: $name depth $d: expected $exp, got $got"; fail=1; fi
    d=$((d+1))
  done
}

check startpos   "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1" 20 400 8902 197281 4865609
check kiwipete   "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1" 48 2039 97862 4085603
check position3  "8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - - 0 1" 14 191 2812 43238 674624
check position4  "r3k2r/Pppp1ppp/1b3nbN/nP6/BBP1P3/q4N2/Pp1P2PP/R2Q1RK1 w kq - 0 1" 6 264 9467 422333
check position4m "r2q1rk1/pP1p2pp/Q4n2/bbp1p3/Np6/1B3NBn/pPPP1PPP/R3K2R b KQ - 0 1" 6 264 9467 422333
check position5  "rnbq1k1r/pp1Pbppp/2p5/8/2B5/8/PPP1NnPP/RNBQK2R w KQ - 1 8" 44 1486 62379 2103487
check position6  "r4rk1/1pp1qppp/p1np1n2/2b1p1B1/2B1P1b1/P1NP1N2/1PP1QPPP/R4RK1 w - - 0 10" 46 2079 89890 3894594

[ "$fail" -eq 0 ] && echo "All perft checks passed." || { echo "Perft failures."; exit 1; }
