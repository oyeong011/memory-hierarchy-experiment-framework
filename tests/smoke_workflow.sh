#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BIN_DIR="$ROOT_DIR/bin"
TMP_DIR="$(mktemp -d "${TMPDIR:-/tmp}/memory-workflow-smoke.XXXXXX")"

cleanup() {
  rm -rf "$TMP_DIR"
}
trap cleanup EXIT

validate_benchmark_csv() {
  local csv_path="$1"
  local expected_benchmark="$2"
  local expected_variant="$3"

  python3 - "$csv_path" "$expected_benchmark" "$expected_variant" <<'PY'
from __future__ import annotations

import csv
import sys
from pathlib import Path

csv_path = Path(sys.argv[1])
expected_benchmark = sys.argv[2]
expected_variant = sys.argv[3]
expected_columns = 10

rows = list(csv.reader(csv_path.open(newline="")))
if len(rows) != 1:
    raise SystemExit(f"{csv_path}: expected 1 CSV row, found {len(rows)}")

row = rows[0]
if len(row) != expected_columns:
    raise SystemExit(f"{csv_path}: expected {expected_columns} columns, found {len(row)}")

if row[0] != expected_benchmark:
    raise SystemExit(f"{csv_path}: benchmark field {row[0]!r} != {expected_benchmark!r}")

if row[1] != expected_variant:
    raise SystemExit(f"{csv_path}: variant field {row[1]!r} != {expected_variant!r}")

for index, label in ((8, "elapsed_sec"), (9, "extra")):
    try:
        value = float(row[index])
    except ValueError as exc:
        raise SystemExit(f"{csv_path}: column {label} is not numeric: {row[index]!r}") from exc
    if value <= 0:
        raise SystemExit(f"{csv_path}: column {label} must be positive, found {value}")

PY
}

run_case() {
  local label="$1"
  local expected_benchmark="$2"
  local expected_variant="$3"
  shift 3

  local out_path="$TMP_DIR/${label}.csv"
  "$@" >"$out_path"
  validate_benchmark_csv "$out_path" "$expected_benchmark" "$expected_variant"
  printf 'ok %s\n' "$label"
}

validate_manifest_schema() {
  local manifest_path="$1"
  local parsed_path="$2"

  python3 - "$manifest_path" "$parsed_path" <<'PY'
from __future__ import annotations

import csv
import sys
from pathlib import Path

manifest_path = Path(sys.argv[1])
parsed_path = Path(sys.argv[2])

expected_header = [
    "run_id",
    "benchmark",
    "variant",
    "size_bytes",
    "elements",
    "rows",
    "cols",
    "stride",
    "tile_size",
    "repeat_idx",
    "core_id",
    "elapsed_sec",
    "extra",
    "cycles",
    "instructions",
    "cache_references",
    "cache_misses",
    "branch_misses",
    "l1_dcache_loads",
    "l1_dcache_load_misses",
    "llc_loads",
    "llc_load_misses",
    "dtlb_load_misses",
    "task_clock_msec",
    "cpu_clock_msec",
    "page_faults",
    "context_switches",
    "cpu_migrations",
    "perf_repeat",
    "stdout_path",
    "perf_path",
    "status",
]

manifest_rows = list(csv.reader(manifest_path.open(newline="")))
parsed_rows = list(csv.reader(parsed_path.open(newline="")))

if not manifest_rows:
    raise SystemExit(f"{manifest_path}: empty manifest")

if not parsed_rows:
    raise SystemExit(f"{parsed_path}: empty parsed CSV")

manifest_header = manifest_rows[0]
parsed_header = parsed_rows[0]
if parsed_header != expected_header:
    raise SystemExit(f"{parsed_path}: unexpected header\nexpected: {expected_header}\nactual:   {parsed_header}")

if len(parsed_rows) - 1 != len(manifest_rows) - 1:
    raise SystemExit(
        f"{parsed_path}: expected {len(manifest_rows) - 1} data rows, found {len(parsed_rows) - 1}"
    )

if any(len(row) != len(expected_header) for row in parsed_rows[1:]):
    bad = next((row for row in parsed_rows[1:] if len(row) != len(expected_header)), None)
    raise SystemExit(f"{parsed_path}: malformed row length: {bad}")

if len(manifest_header) != 17:
    raise SystemExit(f"{manifest_path}: expected 17 manifest columns, found {len(manifest_header)}")

PY
}

validate_plot_outputs() {
  local plots_dir="$1"

  python3 - "$plots_dir" <<'PY'
from __future__ import annotations

import sys
from pathlib import Path

plots_dir = Path(sys.argv[1])
required = [
    "latency_staircase.png",
    "bandwidth_vs_working_set.png",
    "row_vs_column_traversal.png",
    "tile_size_effect.png",
]

for name in required:
    path = plots_dir / name
    if not path.exists() or path.stat().st_size == 0:
        raise SystemExit(f"missing plot output: {path}")

stride_candidates = [
    plots_dir / "cache_miss_rate_vs_stride.png",
    plots_dir / "stride_access_cost_vs_stride.png",
]
if not any(path.exists() and path.stat().st_size > 0 for path in stride_candidates):
    raise SystemExit("missing stride plot output")

PY
}

main() {
  echo "Building benchmarks..."
  make -C "$ROOT_DIR" all

  echo "Running benchmark smoke cases..."
  run_case pointer_chase pointer_chase chase "$BIN_DIR/pointer_chase" 64 10
  run_case stream_lite_copy stream_lite copy "$BIN_DIR/stream_lite" 64 copy 5
  run_case stream_lite_triad stream_lite triad "$BIN_DIR/stream_lite" 64 triad 5
  run_case stride_access stride_access sum "$BIN_DIR/stride_access" 64 4 5
  run_case row_col_row row_col_traversal row "$BIN_DIR/row_col_traversal" 8 8 row 3
  run_case row_col_col row_col_traversal col "$BIN_DIR/row_col_traversal" 8 8 col 3
  if [[ -x "$BIN_DIR/matmul_tiled" ]]; then
    run_case matmul_tiled matmul_tiled tiled "$BIN_DIR/matmul_tiled" 8 4 2
  fi

  echo "Checking parse_perf.py on sample manifest..."
  local sample_manifest="$ROOT_DIR/results/raw/smoke_parallel_20260406/manifest.csv"
  if [[ ! -f "$sample_manifest" ]]; then
    sample_manifest="$ROOT_DIR/results/raw/manifest.csv"
  fi
  if [[ ! -f "$sample_manifest" ]]; then
    echo "No sample manifest found under results/raw/" >&2
    exit 1
  fi

  local parsed_csv="$TMP_DIR/parsed_perf.csv"
  python3 "$ROOT_DIR/scripts/parse_perf.py" --manifest "$sample_manifest" --output "$parsed_csv"
  validate_manifest_schema "$sample_manifest" "$parsed_csv"
  printf 'ok parse_perf\n'

  echo "Checking plot_results.py on sample processed data..."
  local plots_dir="$TMP_DIR/plots"
  local derived_csv="$TMP_DIR/perf_results_derived.csv"
  python3 "$ROOT_DIR/scripts/plot_results.py" \
    --input "$ROOT_DIR/results/processed/perf_results_with_matmul.csv" \
    --plots-dir "$plots_dir" \
    --derived-output "$derived_csv"
  validate_plot_outputs "$plots_dir"
  if [[ ! -s "$derived_csv" ]]; then
    echo "derived output missing or empty: $derived_csv" >&2
    exit 1
  fi
  printf 'ok plot_results\n'

  echo "Smoke workflow completed successfully."
}

main "$@"
