# Memory Hierarchy Experiment Framework

A Linux benchmarking framework for studying memory bottlenecks, cache locality, TLB-related behavior, and matrix blocking effects using C microbenchmarks and `perf`-based automation.

## Overview

This project was built to analyze how memory-access patterns and working-set sizes affect performance on Linux systems. It combines:
- C microbenchmarks
- Bash automation scripts
- Python parsing and plotting tools
- `perf`-based performance measurement

## Core Experiments

### 1. Memory access patterns
- sequential access
- strided access
- random access

### 2. Matrix multiplication variants
- naive loop order
- reordered loop order
- blocked / tiled execution

### 3. Virtual memory behavior
- page-level sequential access
- page-level random access

## Repository Structure

```text
src/        benchmark implementations
scripts/    build, collection, parsing, plotting automation
docs/       workflow and reproducibility notes
report/     final write-up and figures
tests/      smoke test
archive/    non-essential supporting materials kept out of the main path
```

## Build

```bash
make
```

## Run

Collect perf results:

```bash
./scripts/collect_perf.sh
```

Parse one run manifest:

```bash
python3 ./scripts/parse_perf.py \
  --manifest results/raw/<run-id>/manifest.csv \
  --output results/processed/perf_results.csv
```

Generate plots:

```bash
python3 ./scripts/plot_results.py \
  --input results/processed/perf_results.csv \
  --plots-dir plots
```

If hardware PMU events are unavailable, use:

```bash
./scripts/collect_bench_only.sh
```

## Key Focus

- cache locality and working-set transitions
- memory bottlenecks under different access patterns
- TLB-related behavior at page granularity
- matrix blocking effects on cache reuse

## Result Summary

This framework enables reproducible comparison of memory-access patterns, matrix traversal orders, and page-level access behavior using Linux `perf` counters such as cycles, instructions, cache misses, page faults, and `dTLB-load-misses`.

## Notes

- The main artifacts intended for review are under `src/`, `scripts/`, `docs/`, and `report/`.
- Supporting materials that are not essential to the core benchmarking workflow were moved under `archive/` to keep the repository focused.
