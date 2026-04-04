#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BIN_DIR="$ROOT_DIR/bin"
RESULTS_DIR="$ROOT_DIR/results"
PLOTS_DIR="$ROOT_DIR/plots"

ARRAY_SIZES=(${ARRAY_SIZES:-1M 4M 16M 64M 256M 1G})
STRIDES=(${STRIDES:-1 4 16 64 256})
MATRIX_SIZES=(${MATRIX_SIZES:-256 512 1024})
BLOCK_SIZES=(${BLOCK_SIZES:-16 32 64})
MEMORY_PASSES="${MEMORY_PASSES:-8}"
VM_PASSES="${VM_PASSES:-4}"
PERF_EVENTS="cycles,instructions,cache-references,cache-misses,page-faults,dTLB-load-misses"

mkdir -p "$RESULTS_DIR" "$PLOTS_DIR"

if ! command -v perf >/dev/null 2>&1; then
  echo "perf is required but was not found in PATH" >&2
  exit 1
fi

perf_probe_stdout="$(mktemp)"
perf_probe_stderr="$(mktemp)"
if ! perf stat -x, -e cycles -- true >"$perf_probe_stdout" 2>"$perf_probe_stderr"; then
  echo "perf is installed but not usable for the current user." >&2
  cat "$perf_probe_stderr" >&2
  rm -f "$perf_probe_stdout" "$perf_probe_stderr"
  exit 1
fi
rm -f "$perf_probe_stdout" "$perf_probe_stderr"

"$ROOT_DIR/scripts/build_benchmarks.sh"

MEMORY_CSV="$RESULTS_DIR/memory_access.csv"
MATRIX_CSV="$RESULTS_DIR/matrix.csv"
VM_CSV="$RESULTS_DIR/virtual_memory.csv"

printf '%s\n' \
  'experiment,mode,array_size_bytes,stride_elements,passes,time_seconds,cycles,instructions,cache_references,cache_misses,page_faults,dtlb_load_misses,sink' \
  >"$MEMORY_CSV"
printf '%s\n' \
  'experiment,variant,n,block_size,time_seconds,cycles,instructions,cache_references,cache_misses,page_faults,dtlb_load_misses,checksum' \
  >"$MATRIX_CSV"
printf '%s\n' \
  'experiment,mode,bytes,page_size,pages,passes,time_seconds,cycles,instructions,cache_references,cache_misses,page_faults,dtlb_load_misses,minor_faults,major_faults,checksum' \
  >"$VM_CSV"

extract_kv() {
  local file="$1"
  local key="$2"
  awk -F= -v target="$key" '$1 == target {print $2; exit}' "$file"
}

extract_perf_value() {
  local file="$1"
  local event="$2"
  awk -F, -v target="$event" '
    $3 == target {
      gsub(/[[:space:]]/, "", $1)
      if ($1 == "<notcounted>" || $1 == "<notsupported>" || $1 == "") {
        print ""
      } else {
        print $1
      }
      exit
    }
  ' "$file"
}

run_with_perf() {
  local stdout_file="$1"
  local perf_file="$2"
  shift 2

  perf stat -x, -e "$PERF_EVENTS" -- "$@" >"$stdout_file" 2>"$perf_file"
}

append_memory_row() {
  local stdout_file="$1"
  local perf_file="$2"
  local mode array_size stride passes time_seconds cycles instructions cache_refs cache_misses page_faults dtlb_misses sink

  mode="$(extract_kv "$stdout_file" mode)"
  array_size="$(extract_kv "$stdout_file" array_bytes)"
  stride="$(extract_kv "$stdout_file" stride_elements)"
  passes="$(extract_kv "$stdout_file" passes)"
  time_seconds="$(extract_kv "$stdout_file" elapsed_seconds)"
  sink="$(extract_kv "$stdout_file" sink)"
  cycles="$(extract_perf_value "$perf_file" cycles)"
  instructions="$(extract_perf_value "$perf_file" instructions)"
  cache_refs="$(extract_perf_value "$perf_file" cache-references)"
  cache_misses="$(extract_perf_value "$perf_file" cache-misses)"
  page_faults="$(extract_perf_value "$perf_file" page-faults)"
  dtlb_misses="$(extract_perf_value "$perf_file" dTLB-load-misses)"

  printf '%s\n' \
    "memory_access,$mode,$array_size,$stride,$passes,$time_seconds,$cycles,$instructions,$cache_refs,$cache_misses,$page_faults,$dtlb_misses,$sink" \
    >>"$MEMORY_CSV"
}

append_matrix_row() {
  local stdout_file="$1"
  local perf_file="$2"
  local variant n block_size time_seconds cycles instructions cache_refs cache_misses page_faults dtlb_misses checksum

  variant="$(extract_kv "$stdout_file" variant)"
  n="$(extract_kv "$stdout_file" n)"
  block_size="$(extract_kv "$stdout_file" block_size)"
  time_seconds="$(extract_kv "$stdout_file" elapsed_seconds)"
  checksum="$(extract_kv "$stdout_file" checksum)"
  cycles="$(extract_perf_value "$perf_file" cycles)"
  instructions="$(extract_perf_value "$perf_file" instructions)"
  cache_refs="$(extract_perf_value "$perf_file" cache-references)"
  cache_misses="$(extract_perf_value "$perf_file" cache-misses)"
  page_faults="$(extract_perf_value "$perf_file" page-faults)"
  dtlb_misses="$(extract_perf_value "$perf_file" dTLB-load-misses)"

  printf '%s\n' \
    "matrix,$variant,$n,$block_size,$time_seconds,$cycles,$instructions,$cache_refs,$cache_misses,$page_faults,$dtlb_misses,$checksum" \
    >>"$MATRIX_CSV"
}

append_vm_row() {
  local stdout_file="$1"
  local perf_file="$2"
  local mode bytes page_size pages passes time_seconds cycles instructions cache_refs cache_misses page_faults dtlb_misses minor_faults major_faults checksum

  mode="$(extract_kv "$stdout_file" mode)"
  bytes="$(extract_kv "$stdout_file" bytes)"
  page_size="$(extract_kv "$stdout_file" page_size)"
  pages="$(extract_kv "$stdout_file" pages)"
  passes="$(extract_kv "$stdout_file" passes)"
  time_seconds="$(extract_kv "$stdout_file" elapsed_seconds)"
  minor_faults="$(extract_kv "$stdout_file" minor_faults)"
  major_faults="$(extract_kv "$stdout_file" major_faults)"
  checksum="$(extract_kv "$stdout_file" checksum)"
  cycles="$(extract_perf_value "$perf_file" cycles)"
  instructions="$(extract_perf_value "$perf_file" instructions)"
  cache_refs="$(extract_perf_value "$perf_file" cache-references)"
  cache_misses="$(extract_perf_value "$perf_file" cache-misses)"
  page_faults="$(extract_perf_value "$perf_file" page-faults)"
  dtlb_misses="$(extract_perf_value "$perf_file" dTLB-load-misses)"

  printf '%s\n' \
    "virtual_memory,$mode,$bytes,$page_size,$pages,$passes,$time_seconds,$cycles,$instructions,$cache_refs,$cache_misses,$page_faults,$dtlb_misses,$minor_faults,$major_faults,$checksum" \
    >>"$VM_CSV"
}

echo "Running memory access experiments..."
for size in "${ARRAY_SIZES[@]}"; do
  stdout_file="$(mktemp)"
  perf_file="$(mktemp)"
  run_with_perf "$stdout_file" "$perf_file" \
    "$BIN_DIR/memory_access_bench" --mode seq --size "$size" --passes "$MEMORY_PASSES"
  append_memory_row "$stdout_file" "$perf_file"
  rm -f "$stdout_file" "$perf_file"

  for stride in "${STRIDES[@]}"; do
    stdout_file="$(mktemp)"
    perf_file="$(mktemp)"
    run_with_perf "$stdout_file" "$perf_file" \
      "$BIN_DIR/memory_access_bench" --mode stride --size "$size" --stride "$stride" --passes "$MEMORY_PASSES"
    append_memory_row "$stdout_file" "$perf_file"
    rm -f "$stdout_file" "$perf_file"
  done

  stdout_file="$(mktemp)"
  perf_file="$(mktemp)"
  run_with_perf "$stdout_file" "$perf_file" \
    "$BIN_DIR/memory_access_bench" --mode rand --size "$size" --passes "$MEMORY_PASSES" --seed 12345
  append_memory_row "$stdout_file" "$perf_file"
  rm -f "$stdout_file" "$perf_file"
done

echo "Running matrix experiments..."
for n in "${MATRIX_SIZES[@]}"; do
  stdout_file="$(mktemp)"
  perf_file="$(mktemp)"
  run_with_perf "$stdout_file" "$perf_file" \
    "$BIN_DIR/matrix_bench" --variant naive --n "$n"
  append_matrix_row "$stdout_file" "$perf_file"
  rm -f "$stdout_file" "$perf_file"

  stdout_file="$(mktemp)"
  perf_file="$(mktemp)"
  run_with_perf "$stdout_file" "$perf_file" \
    "$BIN_DIR/matrix_bench" --variant reordered --n "$n"
  append_matrix_row "$stdout_file" "$perf_file"
  rm -f "$stdout_file" "$perf_file"

  for block_size in "${BLOCK_SIZES[@]}"; do
    stdout_file="$(mktemp)"
    perf_file="$(mktemp)"
    run_with_perf "$stdout_file" "$perf_file" \
      "$BIN_DIR/matrix_bench" --variant blocked --n "$n" --block "$block_size"
    append_matrix_row "$stdout_file" "$perf_file"
    rm -f "$stdout_file" "$perf_file"
  done
done

echo "Running virtual memory experiments..."
for size in "${ARRAY_SIZES[@]}"; do
  for mode in seq rand; do
    stdout_file="$(mktemp)"
    perf_file="$(mktemp)"
    run_with_perf "$stdout_file" "$perf_file" \
      "$BIN_DIR/virtual_memory_bench" --mode "$mode" --size "$size" --passes "$VM_PASSES" --seed 12345
    append_vm_row "$stdout_file" "$perf_file"
    rm -f "$stdout_file" "$perf_file"
  done
done

echo "Results written to:"
echo "  $MEMORY_CSV"
echo "  $MATRIX_CSV"
echo "  $VM_CSV"
