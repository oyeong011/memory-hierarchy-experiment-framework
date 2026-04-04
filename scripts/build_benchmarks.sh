#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BIN_DIR="$ROOT_DIR/bin"
SRC_DIR="$ROOT_DIR/src"

mkdir -p "$BIN_DIR"

CC="${CC:-cc}"
CFLAGS=(
  -O2
  -std=c11
  -Wall
  -Wextra
  -Wpedantic
  -march=native
)

"$CC" "${CFLAGS[@]}" -o "$BIN_DIR/memory_access_bench" "$SRC_DIR/memory_access_bench.c"
"$CC" "${CFLAGS[@]}" -o "$BIN_DIR/matrix_bench" "$SRC_DIR/matrix_bench.c"
"$CC" "${CFLAGS[@]}" -o "$BIN_DIR/virtual_memory_bench" "$SRC_DIR/virtual_memory_bench.c"

printf 'Built benchmarks in %s\n' "$BIN_DIR"
