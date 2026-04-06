#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BIN_DIR="$ROOT_DIR/bin"
RAW_BASE_DIR="${RAW_BASE_DIR:-$ROOT_DIR/results/raw}"
RUN_LABEL="${RUN_LABEL:-run_$(date +%Y%m%d_%H%M%S)_$$}"
RUN_DIR="${RUN_DIR:-$RAW_BASE_DIR/$RUN_LABEL}"
MANIFEST_PATH="$RUN_DIR/manifest.csv"

HARDWARE_CANDIDATE_EVENTS=(
  cycles
  instructions
  cache-references
  cache-misses
  branch-misses
  L1-dcache-loads
  L1-dcache-load-misses
  LLC-loads
  LLC-load-misses
  dTLB-load-misses
)

SOFTWARE_CANDIDATE_EVENTS=(
  task-clock
  cpu-clock
  page-faults
  context-switches
  cpu-migrations
)

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
PERF_REPEAT="${PERF_REPEAT:-1}"
CORE_ID="${CORE_ID:-0}"
WARMUP="${WARMUP:-1}"
BUILD_FIRST="${BUILD_FIRST:-1}"
RUN_SEQ=0

warn() {
  printf 'WARN: %s\n' "$*" >&2
}

die() {
  printf 'ERROR: %s\n' "$*" >&2
  exit 1
}

probe_perf_access() {
  local probe_file
  probe_file="$(mktemp)"

  if ! perf stat -x ';' --no-big-num -e task-clock -- true >/dev/null 2>"${probe_file}"; then
    cat "${probe_file}" >&2
    rm -f "${probe_file}"
    die "perf is installed but not usable for the current user."
  fi

  rm -f "${probe_file}"
}

probe_event_group() {
  local group_name="$1"
  shift
  local event
  local probe_file
  local -n out_array="$group_name"

  out_array=()
  for event in "$@"; do
    probe_file="$(mktemp)"
    if perf stat -x ';' --no-big-num -e "${event}" -- true >/dev/null 2>"${probe_file}"; then
      if grep -qiE 'not supported|unknown|not counted|syntax error' "${probe_file}"; then
        warn "Skipping unsupported event: ${event}"
      else
        out_array+=("${event}")
      fi
    else
      warn "Skipping unavailable event: ${event}"
    fi
    rm -f "${probe_file}"
  done
}

discover_supported_events() {
  HARDWARE_EVENTS=()
  SOFTWARE_EVENTS=()
  SUPPORTED_EVENTS=()

  probe_event_group HARDWARE_EVENTS "${HARDWARE_CANDIDATE_EVENTS[@]}"
  probe_event_group SOFTWARE_EVENTS "${SOFTWARE_CANDIDATE_EVENTS[@]}"

  if [[ "${#HARDWARE_EVENTS[@]}" -eq 0 ]]; then
    warn "No hardware PMU events are available on this machine; collecting software perf events only."
  fi

  SUPPORTED_EVENTS=("${HARDWARE_EVENTS[@]}" "${SOFTWARE_EVENTS[@]}")
  if [[ "${#SUPPORTED_EVENTS[@]}" -eq 0 ]]; then
    die "No perf events were usable on this machine."
  fi
}

normalize_compat_env() {
  # Keep older and ad-hoc variable names working.
  if [[ -n "${TRAVERSAL_VARIANTS:-}" && "${TRAVERSAL_ORDERS}" == "row col" ]]; then
    TRAVERSAL_ORDERS="${TRAVERSAL_VARIANTS}"
  fi
  if [[ -n "${TILE_SIZES:-}" && "${MATMUL_TILE_SIZES}" == "8 16 32 64" ]]; then
    MATMUL_TILE_SIZES="${TILE_SIZES}"
  fi
}

write_manifest_header() {
  mkdir -p "${RUN_DIR}"
  printf '%s\n' \
    'run_id,benchmark,variant,size_bytes,elements,rows,cols,stride,tile_size,elapsed_sec,extra,repeat_idx,core_id,perf_repeat,stdout_path,perf_path,status' \
    >"${MANIFEST_PATH}"
}

append_manifest_row() {
  local run_id="$1"
  local benchmark="$2"
  local variant="$3"
  local size_bytes="$4"
  local elements="$5"
  local rows="$6"
  local cols="$7"
  local stride="$8"
  local tile_size="$9"
  local elapsed_sec="${10}"
  local extra="${11}"
  local repeat_idx="${12}"
  local stdout_path="${13}"
  local perf_path="${14}"
  local status="${15}"

  printf '%s\n' \
    "${run_id},${benchmark},${variant},${size_bytes},${elements},${rows},${cols},${stride},${tile_size},${elapsed_sec},${extra},${repeat_idx},${CORE_ID},${PERF_REPEAT},${stdout_path},${perf_path},${status}" \
    >>"${MANIFEST_PATH}"
}

record_run() {
  local benchmark_label="$1"
  shift

  local rep
  for ((rep = 0; rep < OUTER_REPEATS; ++rep)); do
    local run_id
    local stdout_path
    local perf_path
    local csv_line=""
    local benchmark=""
    local variant=""
    local size_bytes="0"
    local elements="0"
    local rows="0"
    local cols="0"
    local stride="0"
    local tile_size="0"
    local elapsed_sec="0"
    local extra="0"
    local status="ok"
    local -a run_cmd=("$@")

    RUN_SEQ=$((RUN_SEQ + 1))
    run_id="$(printf '%04d_%s_%s_r%02d' "${RUN_SEQ}" "${benchmark_label}" "$(date +%Y%m%d_%H%M%S)" "${rep}")"
    stdout_path="${RUN_DIR}/${run_id}.bench.csv"
    perf_path="${RUN_DIR}/${run_id}.perf.csv"

    if [[ -n "${CORE_ID}" ]] && command -v taskset >/dev/null 2>&1; then
      run_cmd=(taskset -c "${CORE_ID}" "${run_cmd[@]}")
    fi

    if [[ "${WARMUP}" == "1" ]]; then
      "${run_cmd[@]}" >/dev/null 2>&1 || true
    fi

    if ! perf stat -x ';' --no-big-num -r "${PERF_REPEAT}" -e "${SUPPORTED_EVENTS_JOINED}" -- \
      "${run_cmd[@]}" >"${stdout_path}" 2>"${perf_path}"; then
      status="perf_or_command_failed"
      warn "Run failed for ${benchmark_label} repeat ${rep}; keeping raw logs"
    fi

    if [[ -s "${stdout_path}" ]]; then
      IFS=',' read -r benchmark variant size_bytes elements rows cols stride tile_size elapsed_sec extra <"${stdout_path}" || true
    else
      benchmark="${benchmark_label}"
      variant="unknown"
      status="missing_stdout"
    fi

    append_manifest_row \
      "${run_id}" \
      "${benchmark}" \
      "${variant}" \
      "${size_bytes}" \
      "${elements}" \
      "${rows}" \
      "${cols}" \
      "${stride}" \
      "${tile_size}" \
      "${elapsed_sec}" \
      "${extra}" \
      "${rep}" \
      "${stdout_path}" \
      "${perf_path}" \
      "${status}"
  done
}

main() {
  local -a pointer_sizes stream_sizes stride_sizes stride_values traversal_shapes traversal_orders stream_variants matmul_shapes matmul_tiles

  command -v perf >/dev/null 2>&1 || die "perf not found in PATH"
  command -v python3 >/dev/null 2>&1 || die "python3 not found in PATH"

  if [[ "${BUILD_FIRST}" == "1" ]]; then
    make -C "${ROOT_DIR}" all
  fi

  probe_perf_access
  normalize_compat_env
  discover_supported_events
  SUPPORTED_EVENTS_JOINED="$(IFS=,; printf '%s' "${SUPPORTED_EVENTS[*]}")"

  [[ -n "${POINTER_SIZES}" ]] && read -r -a pointer_sizes <<<"${POINTER_SIZES}" || pointer_sizes=()
  [[ -n "${STREAM_SIZES}" ]] && read -r -a stream_sizes <<<"${STREAM_SIZES}" || stream_sizes=()
  [[ -n "${STRIDE_SIZES}" ]] && read -r -a stride_sizes <<<"${STRIDE_SIZES}" || stride_sizes=()
  [[ -n "${STRIDE_VALUES}" ]] && read -r -a stride_values <<<"${STRIDE_VALUES}" || stride_values=()
  [[ -n "${TRAVERSAL_SHAPES}" ]] && read -r -a traversal_shapes <<<"${TRAVERSAL_SHAPES}" || traversal_shapes=()
  [[ -n "${TRAVERSAL_ORDERS}" ]] && read -r -a traversal_orders <<<"${TRAVERSAL_ORDERS}" || traversal_orders=()
  [[ -n "${STREAM_VARIANTS}" ]] && read -r -a stream_variants <<<"${STREAM_VARIANTS}" || stream_variants=()
  [[ -n "${MATMUL_SHAPES}" ]] && read -r -a matmul_shapes <<<"${MATMUL_SHAPES}" || matmul_shapes=()
  [[ -n "${MATMUL_TILE_SIZES}" ]] && read -r -a matmul_tiles <<<"${MATMUL_TILE_SIZES}" || matmul_tiles=()

  write_manifest_header

  if [[ "${#pointer_sizes[@]}" -gt 0 ]]; then
    for elements in "${pointer_sizes[@]}"; do
      record_run "pointer_chase" "${BIN_DIR}/pointer_chase" "${elements}"
    done
  fi

  if [[ "${#stream_sizes[@]}" -gt 0 && "${#stream_variants[@]}" -gt 0 ]]; then
    for elements in "${stream_sizes[@]}"; do
      for variant in "${stream_variants[@]}"; do
        record_run "stream_lite_${variant}" "${BIN_DIR}/stream_lite" "${elements}" "${variant}"
      done
    done
  fi

  if [[ "${#stride_sizes[@]}" -gt 0 && "${#stride_values[@]}" -gt 0 ]]; then
    for elements in "${stride_sizes[@]}"; do
      for stride in "${stride_values[@]}"; do
        record_run "stride_access_s${stride}" "${BIN_DIR}/stride_access" "${elements}" "${stride}"
      done
    done
  fi

  if [[ "${#traversal_shapes[@]}" -gt 0 && "${#traversal_orders[@]}" -gt 0 ]]; then
    for shape in "${traversal_shapes[@]}"; do
      rows="${shape%x*}"
      cols="${shape#*x}"
      for order in "${traversal_orders[@]}"; do
        record_run "row_col_${order}_${rows}x${cols}" "${BIN_DIR}/row_col_traversal" "${rows}" "${cols}" "${order}"
      done
    done
  fi

  if [[ -x "${BIN_DIR}/matmul_tiled" && "${#matmul_shapes[@]}" -gt 0 && "${#matmul_tiles[@]}" -gt 0 ]]; then
    local shape n
    for shape in "${matmul_shapes[@]}"; do
      if [[ "${shape}" == *x* ]]; then
        n="${shape%x*}"
      else
        n="${shape}"
      fi
      for tile in "${matmul_tiles[@]}"; do
        if (( tile <= n )); then
          record_run "matmul_tiled_n${n}_t${tile}" "${BIN_DIR}/matmul_tiled" "${n}" "${tile}"
        fi
      done
    done
  fi

  printf 'Run directory: %s\n' "${RUN_DIR}"
  printf 'Manifest: %s\n' "${MANIFEST_PATH}"
  printf 'Supported events: %s\n' "${SUPPORTED_EVENTS_JOINED}"
  printf 'Parse next with: python3 %s/scripts/parse_perf.py --manifest %s --output %s/results/processed/perf_results.csv\n' \
    "${ROOT_DIR}" "${MANIFEST_PATH}" "${ROOT_DIR}"
}

main "$@"
