#!/bin/sh
# Builds and runs the host-side game rule tests.
set -e
cd "$(dirname "$0")/.."
mkdir -p build/tests
cc -std=gnu99 -Wall -Wextra -Wno-unused-parameter \
  -Itests/shim -Isrc/c \
  tests/test_game.c tests/shim/pebble_shim.c src/c/game.c \
  -o build/tests/test_game
./build/tests/test_game
