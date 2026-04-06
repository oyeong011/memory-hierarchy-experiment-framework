# perf Workflow

## Files

- Collector: `scripts/collect_perf.sh`
- Parser: `scripts/parse_perf.py`
- Parsed CSV: `results/processed/perf_results.csv`
- Optional merged CSV with tiled-matmul rows: `results/processed/perf_results_with_matmul.csv`

## CSV Schema

The parsed CSV contains these columns:

```text
run_id,benchmark,variant,size_bytes,elements,rows,cols,stride,tile_size,repeat_idx,core_id,elapsed_sec,extra,cycles,instructions,cache_references,cache_misses,branch_misses,l1_dcache_loads,l1_dcache_load_misses,llc_loads,llc_load_misses,dtlb_load_misses,task_clock_msec,cpu_clock_msec,page_faults,context_switches,cpu_migrations,perf_repeat,stdout_path,perf_path,status
```

## Usage

Build the benchmarks:

```bash
make
```

Collect raw perf outputs and a manifest:

```bash
CORE_ID=2 OUTER_REPEATS=5 PERF_REPEAT=1 ./scripts/collect_perf.sh
```

Pinning is done through `taskset -c "$CORE_ID"` when `taskset` is available.
Each collector invocation writes to a unique run directory under `results/raw/`.

Parse the raw logs into a single CSV:

```bash
python3 scripts/parse_perf.py \
  --manifest results/raw/<run-id>/manifest.csv \
  --output results/processed/perf_results.csv
```

If the machine blocks `perf`, use the benchmark-only fallback:

```bash
./scripts/collect_bench_only.sh
python3 scripts/plot_results.py \
  --input results/processed/bench_only_results.csv \
  --plots-dir plots
```

If you ran the optional tiled matrix-multiply benchmark and want a single
analysis table, merge that output into the parsed CSV before plotting. The final
report in this repository uses `results/processed/perf_results_with_matmul.csv`
for that reason.

## Sweep Controls

Override sweep ranges with environment variables:

```bash
POINTER_SIZES="4096 65536 1048576" \
STREAM_SIZES="16384 262144" \
STRIDE_SIZES="65536 1048576" \
STRIDE_VALUES="1 4 16 64 256" \
TRAVERSAL_SHAPES="256x256 512x512" \
MATMUL_SHAPES="128 256" \
MATMUL_TILE_SIZES="8 16 32" \
OUTER_REPEATS=3 \
./scripts/collect_perf.sh
```

## Event Handling

The collector probes candidate events one by one. Unsupported events are skipped
with a warning. If `perf` itself is blocked by permissions, the collector stops
early with the kernel error message.

If the machine exposes no hardware PMU events, the collector falls back to
software events such as `task-clock`, `cpu-clock`, `page-faults`, and
`context-switches`. In that case the hardware counter columns remain empty, but
the runtime/shape data and software event columns are still recorded.

Preferred event set:

- `cycles`
- `instructions`
- `cache-references`
- `cache-misses`
- `branch-misses`
- `L1-dcache-loads`
- `L1-dcache-load-misses`
- `LLC-loads`
- `LLC-load-misses`
- `dTLB-load-misses`

## Reproducibility Checklist

- Keep the machine idle during runs.
- Pin to a fixed core with `CORE_ID`.
- Use the same compiler and optimization flags across all runs.
- Record `uname -a`, compiler version, and CPU model.
- Warm up the benchmark once before each measured run or document if warmup is disabled.
- If possible, fix the CPU frequency governor to `performance`.
- If possible, disable turbo boost or at least document whether it was enabled.
- Check `cat /proc/sys/kernel/perf_event_paranoid` before the experiment.
- Prefer running on AC power and without thermal throttling.
- Repeat the same sweep on the same kernel and hardware configuration.

## Rootless Note

Many distributions block hardware counters for normal users. If you see a
permission error, check:

```bash
cat /proc/sys/kernel/perf_event_paranoid
```

Typical project note:

- `perf_event_paranoid=4` usually blocks the hardware events used here.
- Lower values or `CAP_PERFMON` are typically needed for rootless runs.
