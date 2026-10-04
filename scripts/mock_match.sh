#!/usr/bin/env bash
# Two-game mock match: game 1 MCTS White vs Random Black, game 2 MCTS Black vs Random White.
set -e
cd "$(dirname "$0")/.."
make -s
./bin/segmate --match 2
