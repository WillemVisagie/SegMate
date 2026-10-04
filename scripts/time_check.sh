#!/usr/bin/env bash
# Time-management checks over UCI: movetime/clock budgets are honoured, `nodes`
# limits work, `go infinite` only answers after `stop`, and stop is prompt.
cd "$(dirname "$0")/.."
make -s || exit 1
fail=0
ok()  { echo "PASS: $1"; }
bad() { echo "FAIL: $1"; fail=1; }
now() { perl -MTime::HiRes=time -e 'printf "%d", time*1000'; }
KIWI="position fen r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1"

# run "<commands>" [stop_cmd delay_s]: prints "<elapsed_ms> <bestmove>" for the first bestmove line.
run() {
  local start end out
  start=$(now)
  out=$( { printf "%s\n" "$1"; [ -n "$2" ] && { sleep "$3"; printf "%s\n" "$2"; }; printf 'quit\n'; } \
         | ./bin/segmate --uci | awk '/^bestmove/ {print $2; exit}')
  end=$(now)
  echo "$((end-start)) $out"
}

# 1. movetime 500 -> answers in ~500ms (allow 400-800).
read -r ms mv <<<"$(run "$KIWI
go movetime 500")"
[ "$ms" -ge 400 ] && [ "$ms" -le 800 ] && [ -n "$mv" ] && ok "movetime 500 took ${ms}ms ($mv)" || bad "movetime 500 took ${ms}ms"

# 2. Clock budget: 6000ms left, no increment -> ~6000/30 = 200ms (allow 100-600).
read -r ms mv <<<"$(run "$KIWI
go wtime 6000 btime 6000")"
[ "$ms" -ge 100 ] && [ "$ms" -le 600 ] && ok "wtime 6000 -> ${ms}ms ($mv)" || bad "wtime 6000 took ${ms}ms"

# 3. Tiny clock still answers fast and legally.
read -r ms mv <<<"$(run "$KIWI
go wtime 100 btime 100")"
[ "$ms" -le 400 ] && [ -n "$mv" ] && ok "wtime 100 -> ${ms}ms ($mv)" || bad "wtime 100 took ${ms}ms"

# 4. nodes limit appears in the info line.
n=$(printf '%s\ngo nodes 300\nquit\n' "$KIWI" | ./bin/segmate --uci | awk '/^info/ {for(i=1;i<=NF;i++) if($i=="nodes") print $(i+1)}' | tail -1)
[ "$n" = "300" ] && ok "go nodes 300 searched $n nodes" || bad "go nodes 300 searched '$n'"

# 5. infinite: no bestmove until stop (sent after 1s), then it arrives promptly.
read -r ms mv <<<"$(run "$KIWI
go infinite" "stop" 1)"
[ "$ms" -ge 1000 ] && [ "$ms" -le 1500 ] && [ -n "$mv" ] && ok "go infinite answered ${ms}ms in, only after stop ($mv)" || bad "go infinite: ${ms}ms"

# 6. infinite from a book position still waits for stop.
read -r ms mv <<<"$(run "position startpos
go infinite" "stop" 1)"
[ "$ms" -ge 1000 ] && [ -n "$mv" ] && ok "go infinite (book position) waited for stop" || bad "go infinite (book position): ${ms}ms"

# 7. Mate in one is found within a short movetime.
read -r ms mv <<<"$(run "position fen 6k1/5ppp/8/8/8/8/5PPP/R5K1 w - - 0 1
go movetime 200")"
[ "$mv" = "a1a8" ] && ok "movetime 200 finds mate in 1" || bad "mate in 1: got '$mv'"

# 8. Plain `go` (no limits) still answers (fixed default iterations).
read -r ms mv <<<"$(run "$KIWI
go")"
[ -n "$mv" ] && ok "plain go answers ($mv)" || bad "plain go"

[ "$fail" -eq 0 ] && echo "Time checks passed." || { echo "Time check failures."; exit 1; }
