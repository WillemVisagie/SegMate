#!/usr/bin/env bash
# A/B match in parallel: ./scripts/ab.sh GAMES [--match flags...]
# Splits GAMES over all cores and prints the engine's score with its standard error.
set -e
cd "$(dirname "$0")/.."
games=${1:-100}; shift || true
jobs=$(getconf _NPROCESSORS_ONLN 2>/dev/null || echo 4)
per=$(( (games + jobs - 1) / jobs ))
tmp=$(mktemp -d); cp bin/segmate "$tmp/engine" # snapshot, so rebuilding mid-run is safe
for j in $(seq 1 "$jobs"); do "$tmp/engine" --match "$per" "$@" > "$tmp/$j.out" & done
wait
awk '/^MCTS [0-9]/ { w += $2; l += $5; d += $8 }
     END { n = w + l + d; s = (w + d / 2) / n
           se = sqrt((w * (1 - s) ^ 2 + l * s ^ 2 + d * (0.5 - s) ^ 2) / n / n)
           printf "W %d L %d D %d  score %.1f%% +- %.1f\n", w, l, d, 100 * s, 100 * se }' "$tmp"/*.out
rm -rf "$tmp"
