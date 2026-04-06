#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BIN_DIR="$ROOT_DIR/bin"
OUT_PATH="${OUT_PATH:-$ROOT_DIR/results/processed/bench_only_results.csv}"

POINTER_SIZES="${POINTER_SIZES-4096 16384 65536 262144 1048576 4194304}"
STREAM_SIZES="${STREAM_SIZES-4096 16384 65536 262144 1048576 4194304}"
STREAM_VARIANTS="${STREAM_VARIANTS-copy triad}"
STRIDE_SIZES="${STRIDE_SIZES-4096 16384 65536 262144 1048576}"
STRIDE_VALUES="${STRIDE_VALUES-1 2 4 8 16 32 64 128 256}"
TRAVERSAL_SHAPES="${TRAVERSAL_SHAPES-256x256 512x512 1024x1024}"
TRAVERSAL_ORDERS="${TRAVERSAL_ORDERS-row col}"
MATMUL_SHAPES="${MATMUL_SHAPES-128 256 512}"
MATMUL_TILE_SIZES="${MATMUL_TILE_SIZES-8 16 32 64}"
OUTER_REPEATS="${OUTER_REPEATS:-3}"
BUILD_FIRST="${BUILD_FIRST:-1}"

write_header() {
  mkdir -p "$(dirname "${OUT_PATH}")"
  printf '%s\n' \
    'benchmark,variant,size_bytes,elements,rows,cols,stride,tile_size,repeat_idx,core_id,elapsed_sec,extra,cycles,instructions,cache_references,cache_misses,branch_misses,l1_dcache_loads,l1_dcache_load_misses,llc_loads,llc_load_misses,dtlb_load_misses,task_clock_msec,cpu_clock_msec,page_faults,context_switches,cpu_migrations,status' \
    >"${OUT_PATH}"
}

append_bench_rows() {
  local rep
  local benchmark
  local variant
  local size_bytes
  local elements
  local rows
  local cols
  local stride
  local tile_size
  local elapsed_sec
  local extra
  local line

  for ((rep = 0; rep < OUTER_REPEATS; ++rep)); do
    line="$("$@")"
    IFS=',' read -r benchmark variant size_bytes elements rows cols stride tile_size elapsed_sec extra <<<"${line}"
    printf '%s\n' \
      "${benchmark},${variant},${size_bytes},${elements},${rows},${cols},${stride},${tile_size},${rep},0,${elapsed_sec},${extra},,,,,,,,,,,,,,,bench_only" \
      >>"${OUT_PATH}"
  done
}

main() {
  local -a pointer_sizes stream_sizes stride_sizes stride_values traversal_shapes traversal_orders stream_variants matmul_shapes matmul_tiles

  if [[ "${BUILD_FIRST}" == "1" ]]; then
    make -C "${ROOT_DIR}" all
  fi

  [[ -n "${POINTER_SIZES}" ]] && read -r -a pointer_sizes <<<"${POINTER_SIZES}" || pointer_sizes=()
  [[ -n "${STREAM_SIZES}" ]] && read -r -a stream_sizes <<<"${STREAM_SIZES}" || stream_sizes=()
  [[ -n "${STRIDE_SIZES}" ]] && read -r -a stride_sizes <<<"${STRIDE_SIZES}" || stride_sizes=()
  [[ -n "${STRIDE_VALUES}" ]] && read -r -a stride_values <<<"${STRIDE_VALUES}" || stride_values=()
  [[ -n "${TRAVERSAL_SHAPES}" ]] && read -r -a traversal_shapes <<<"${TRAVERSAL_SHAPES}" || traversal_shapes=()
  [[ -n "${TRAVERSAL_ORDERS}" ]] && read -r -a traversal_orders <<<"${TRAVERSAL_ORDERS}" || traversal_orders=()
  [[ -n "${STREAM_VARIANTS}" ]] && read -r -a stream_variants <<<"${STREAM_VARIANTS}" || stream_variants=()
  [[ -n "${MATMUL_SHAPES}" ]] && read -r -a matmul_shapes <<<"${MATMUL_SHAPES}" || matmul_shapes=()
  [[ -n "${MATMUL_TILE_SIZES}" ]] && read -r -a matmul_tiles <<<"${MATMUL_TILE_SIZES}" || matmul_tiles=()

  write_header

  if [[ "${#pointer_sizes[@]}" -gt 0 ]]; then
    for elements in "${pointer_sizes[@]}"; do
      append_bench_rows "${BIN_DIR}/pointer_chase" "${elements}"
    done
  fi

  if [[ "${#stream_sizes[@]}" -gt 0 && "${#stream_variants[@]}" -gt 0 ]]; then
    for elements in "${stream_sizes[@]}"; do
      for variant in "${stream_variants[@]}"; do
        append_bench_rows "${BIN_DIR}/stream_lite" "${elements}" "${variant}"
      done
    done
  fi

  if [[ "${#stride_sizes[@]}" -gt 0 && "${#stride_values[@]}" -gt 0 ]]; then
    for elements in "${stride_sizes[@]}"; do
      for stride in "${stride_values[@]}"; do
        append_bench_rows "${BIN_DIR}/stride_access" "${elements}" "${stride}"
      done
    done
  fi

  if [[ "${#traversal_shapes[@]}" -gt 0 && "${#traversal_orders[@]}" -gt 0 ]]; then
    for shape in "${traversal_shapes[@]}"; do
      rows="${shape%x*}"
      cols="${shape#*x}"
      for order in "${traversal_orders[@]}"; do
        append_bench_rows "${BIN_DIR}/row_col_traversal" "${rows}" "${cols}" "${order}"
      done
    done
  fi

  if [[ -x "${BIN_DIR}/matmul_tiled" && "${#matmul_shapes[@]}" -gt 0 && "${#matmul_tiles[@]}" -gt 0 ]]; then
    for n in "${matmul_shapes[@]}"; do
      for tile in "${matmul_tiles[@]}"; do
        if (( tile <= n )); then
          append_bench_rows "${BIN_DIR}/matmul_tiled" "${n}" "${tile}"
        fi
      done
    done
  fi

  printf 'Wrote benchmark-only CSV to %s\n' "${OUT_PATH}"
}

main "$@"
