#!/usr/bin/env bash
# Opening book checks: book moves are served, transpositions are found, a position
# outside the book falls through to search, and OwnBook can be disabled.
cd "$(dirname "$0")/.."
make -s || exit 1
fail=0
ok()  { echo "PASS: $1"; }
bad() { echo "FAIL: $1"; fail=1; }

# 1. Startpos: every reply is one of the book's first moves, and more than one occurs.
firsts=" e2e4 d2d4 c2c4 g1f3 b2b3 f2f4 b1c3 g2g3 a2a3 a2a4 b2b4 c2c3 d2d3 e2e3 f2f3 g2g4 h2h3 h2h4 g1h3 b1a3 "
seen=""
for i in $(seq 1 40); do
  mv=$(printf 'position startpos\ngo\nquit\n' | ./bin/segmate --uci | awk '/^bestmove/ {print $2}')
  case "$firsts" in *" $mv "*) ;; *) bad "startpos reply '$mv' is not a book move";; esac
  case "$seen" in *" $mv "*) ;; *) seen="$seen $mv ";; esac
done
[ "$(echo $seen | wc -w)" -gt 1 ] && ok "book varies its first move ($seen)" || bad "no variety"

# 2. Transposition: the Italian Game reached via 1.Nf3 Nc6 2.e4 e5 3.Bc4 Bc5 must give
#    the same replies as the 1.e4 e5 2.Nf3 Nc6 3.Bc4 Bc5 move order (and be book moves).
reply() { printf "position startpos moves $1\ngo\nquit\n" | ./bin/segmate --uci | awk '/^bestmove/ {print $2}'; }
direct=" "; for i in $(seq 1 40); do direct="$direct$(reply 'e2e4 e7e5 g1f3 b8c6 f1c4 f8c5') "; done
mv=$(reply 'g1f3 b8c6 e2e4 e7e5 f1c4 f8c5')
case "$direct" in *" $mv "*) ok "transposition recognised ($mv)";; *) bad "transposition: got '$mv' not in:$direct";; esac

# 3. Out-of-book position still returns a legal move via search.
mv=$(printf 'position fen 6k1/5ppp/8/8/8/8/5PPP/R5K1 w - - 0 1\ngo\nquit\n' | ./bin/segmate --uci | awk '/^bestmove/ {print $2}')
[ "$mv" = "a1a8" ] && ok "out of book -> search (a1a8 mate)" || bad "out of book: got '$mv'"

# 4. OwnBook off: startpos goes to search (just check it still answers legally).
mv=$(printf 'setoption name OwnBook value false\nposition startpos\ngo\nquit\n' | ./bin/segmate --uci | awk '/^bestmove/ {print $2}')
[ -n "$mv" ] && ok "OwnBook=false still answers ($mv)" || bad "OwnBook=false no answer"

[ "$fail" -eq 0 ] && echo "Book checks passed." || { echo "Book check failures."; exit 1; }
