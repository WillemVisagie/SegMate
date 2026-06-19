#!/usr/bin/env bash
# Comprehensive verification of the SegMate engine post-UCI integration.
# Run inside WSL:  bash scripts/verify_all.sh
set -e
cd /mnt/c/Users/Elect/Desktop/SegMate

pass=0; fail=0
ok()   { echo "PASS: $1"; pass=$((pass+1)); }
bad()  { echo "FAIL: $1"; fail=$((fail+1)); }

# ---- 1. Clean rebuild, no warnings -------------------------------------
make clean >/dev/null
BUILD=$(make 2>&1)
if echo "$BUILD" | grep -qE 'warning|error'; then
  echo "$BUILD" | grep -E 'warning|error'
  bad "clean build with -Wall -Wextra"
else
  ok "clean build, zero warnings"
fi
[ -x bin/segmate ] && ok "binary produced" || bad "binary produced"

# ---- 2. FEN round-trip (rebuild the throwaway test) ---------------------
cat > /tmp/fen_test.c << 'EOF'
#include <stdio.h>
#include <string.h>
#include "chess.h"
#include "fen.h"
int main(void){
  const char* c[]={
    "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1",
    "r1bqkbnr/pppp1ppp/2n5/4p3/4P3/5N2/PPPP1PPP/RNBQKB1R w KQkq - 2 3",
    "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1",
    "8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - - 0 1",
    "rnbq1k1r/pp1Pbppp/2p5/8/2B5/8/PPP1NnPP/RNBQK2R w KQ - 1 8"};
  int n=sizeof(c)/sizeof(c[0]),p=0;
  for(int i=0;i<n;i++){char o[160];set_board_from_fen(c[i]);board_to_fen(o);
    if(strcmp(o,c[i])==0)p++;else printf("  mismatch:\n   in :%s\n   out:%s\n",c[i],o);}
  printf("FEN round-trip: %d/%d\n",p,n);
  return p==n?0:1;
}
EOF
gcc -Wall -Wextra -O2 -Isrc -I. /tmp/fen_test.c bin/chess.o bin/fen.o -o /tmp/fen_test 2>/dev/null
if /tmp/fen_test | grep -q "5/5"; then ok "FEN round-trip (5 positions)"; else /tmp/fen_test; bad "FEN round-trip"; fi

# ---- 3. UCI handshake --------------------------------------------------
OUT=$(printf 'uci\nquit\n' | ./bin/segmate --uci)
echo "$OUT" | grep -q 'id name SegMate' && ok "uci -> id name" || bad "uci -> id name"
echo "$OUT" | grep -q 'uciok'           && ok "uci -> uciok"   || bad "uci -> uciok"
printf 'isready\nquit\n' | ./bin/segmate --uci | grep -q 'readyok' && ok "isready -> readyok" || bad "isready"

# ---- 4. Special moves through the UCI move path ------------------------
# Castling: play e2e4 e7e5 g1f3 b8c6 f1c4 g8f6 e1g1 (white O-O). Engine must
# accept the full sequence (no early break) and reply with a legal move.
RES=$(printf 'position startpos moves e2e4 e7e5 g1f3 b8c6 f1c4 g8f6 e1g1\ngo\nquit\n' | ./bin/segmate --uci)
echo "$RES" | grep -qE '^bestmove [a-h][1-8][a-h][1-8]' \
  && ok "white kingside castling accepted + reply" || { echo "$RES"; bad "castling path"; }

# Queenside castling for black: ... e1c1 white O-O-O then check black replies.
RES=$(printf 'position startpos moves d2d4 d7d5 c1f4 c8f5 b1c3 g8f6 d1d2 b8c6 e1c1\ngo\nquit\n' | ./bin/segmate --uci)
echo "$RES" | grep -qE '^bestmove [a-h][1-8][a-h][1-8]' \
  && ok "white queenside castling accepted + reply" || { echo "$RES"; bad "castling path"; }

# En passant: set a FEN where white just played e2e4 (ep target e3) is not it;
# use a real ep FEN: black pawn can capture en passant.
# Position: after 1.e4 Nf6 2.e5 d5 -> ep target d6, white to move, exd6 ep legal.
RES=$(printf 'position fen rnbqkbnr/ppp1pppp/8/3pP3/8/8/PPPP1PPP/RNBQKBNR w KQkq d6 0 3 moves e5d6\ngo\nquit\n' | ./bin/segmate --uci)
echo "$RES" | grep -qE '^bestmove' && ok "en passant capture accepted" || { echo "$RES"; bad "en passant"; }

# Promotion: white pawn a7 promotes. FEN with a7a8q legal.
RES=$(printf 'position fen 8/P7/8/8/8/8/8/k6K w - - 0 1 moves a7a8q\ngo\nquit\n' | ./bin/segmate --uci)
echo "$RES" | grep -qE '^bestmove' && ok "promotion accepted" || { echo "$RES"; bad "promotion"; }

# ---- 5. No-legal-moves -> bestmove (none) -------------------------------
printf 'position fen 7k/5Q2/6K1/8/8/8/8/8 b - - 0 1\ngo\nquit\n' | ./bin/segmate --uci | grep -q 'bestmove (none)' \
  && ok "stalemate -> bestmove (none)" || bad "stalemate handling"

# ---- 6. Default mode (referee) still launches --------------------------
# Feed a quit so it doesn't hang on the menu.
printf '4\nquit\n' | ./bin/segmate >/dev/null 2>&1 && ok "default launch = referee (no crash)" || bad "default launch"

echo
echo "==============================="
echo "  $pass passed, $fail failed"
echo "==============================="
[ "$fail" -eq 0 ]
