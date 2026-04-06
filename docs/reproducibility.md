# Reproducibility Guide

This project is designed to be reproducible on a Linux system with `gcc` or
`clang`, `perf`, `bash`, and `python3`.

## 1. Environment Snapshot

Record these before running the suite:

```bash
uname -a
lscpu
cc --version
perf --version
cat /proc/sys/kernel/perf_event_paranoid
```

If possible, also note the CPU governor and whether turbo is enabled.

## 2. Build

```bash
cd /home/oy/Project
make
```

The default build produces:

- `bin/pointer_chase`
- `bin/stream_lite`
- `bin/stride_access`
- `bin/row_col_traversal`
- optional `bin/matmul_tiled`

## 3. Perf Collection

Run the collector:

```bash
./scripts/collect_perf.sh
```

The collector writes one manifest per invocation under:

```text
results/raw/run_*/manifest.csv
```

Raw benchmark stdout and `perf stat` logs are stored in the same run directory.

Recommended controls:

- Pin to one core with `CORE_ID`.
- Keep `OUTER_REPEATS` fixed across runs.
- Leave the machine otherwise idle if possible.
- Keep compiler flags and binaries unchanged while comparing datasets.

## 4. Parse and Plot

Parse the raw logs into a single CSV:

```bash
python3 ./scripts/parse_perf.py \
  --manifest results/raw/run_<id>/manifest.csv \
  --output results/processed/perf_results.csv
```

Generate plots:

```bash
python3 ./scripts/plot_results.py \
  --input results/processed/perf_results.csv \
  --plots-dir plots
```

If you also ran the optional tiled matrix-multiply benchmark, the plots script
will emit the tile-size figure automatically.

## 5. Fallback When Perf Is Limited

Some systems do not expose the hardware PMU events used for cache and IPC
analysis. If `perf` is blocked or the PMU is unavailable, use:

```bash
./scripts/collect_bench_only.sh
python3 ./scripts/plot_results.py \
  --input results/processed/bench_only_results.csv \
  --plots-dir plots
```

On such systems, hardware-counter columns may remain empty while runtime and
software-event columns are still recorded.

## 6. What To Archive

Keep the following with the final report:

- `results/raw/run_*/manifest.csv`
- `results/processed/perf_results_with_matmul.csv`
- `results/processed/perf_results_with_matmul_derived.csv`
- `plots/`
- `report/figures/`

## 7. Current Host Caveat

On the current host, `perf_event_paranoid` can be lowered but the preferred
hardware PMU events are still reported as unsupported. That means the current
dataset is useful for runtime trends and software events, but it is not suitable
for claims that require hardware counter evidence.
