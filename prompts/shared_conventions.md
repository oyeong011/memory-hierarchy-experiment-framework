# Shared Conventions

Use this file to reduce merge conflicts across parallel sessions.

## Benchmark Names

- `pointer_chase`
- `stream_lite`
- `stride_access`
- `row_col_traversal`
- optional: `matmul_tiled`

## Source Files

- `src/pointer_chase.c`
- `src/stream_lite.c`
- `src/stride_access.c`
- `src/row_col_traversal.c`
- optional: `src/matmul_tiled.c`

## Binary Names

- `bin/pointer_chase`
- `bin/stream_lite`
- `bin/stride_access`
- `bin/row_col_traversal`
- optional: `bin/matmul_tiled`

## CLI Expectations

- All benchmarks accept a problem size argument.
- `pointer_chase`: `./bin/pointer_chase <elements>`
- `stream_lite`: `./bin/stream_lite <elements>`
- `stride_access`: `./bin/stride_access <elements> <stride>`
- `row_col_traversal`: `./bin/row_col_traversal <rows> <cols> <order>`
  - `order`: `row` or `col`

## Benchmark Stdout CSV

Each benchmark should print one CSV line to `stdout` with the same column order.

```text
benchmark,variant,size_bytes,elements,rows,cols,stride,tile_size,elapsed_sec,extra
```

Recommended meanings:

- `benchmark`: canonical benchmark name
- `variant`: operation variant such as `copy`, `triad`, `row`, `col`
- `size_bytes`: total data footprint in bytes
- `elements`: 1D element count when applicable
- `rows`, `cols`: 2D shape when applicable
- `stride`: stride in elements when applicable
- `tile_size`: tile size for optional tiled benchmark
- `elapsed_sec`: wall-clock runtime
- `extra`: benchmark-specific value such as checksum or bandwidth

Unused fields should be emitted as `0` or an empty token consistently.

## perf CSV Schema

Session B and Session C should align on this schema:

```text
benchmark,variant,size_bytes,elements,rows,cols,stride,tile_size,repeat_idx,core_id,elapsed_sec,extra,cycles,instructions,cache_references,cache_misses,branch_misses,l1_dcache_loads,l1_dcache_load_misses,llc_loads,llc_load_misses,dtlb_load_misses,task_clock_msec,cpu_clock_msec,page_faults,context_switches,cpu_migrations,status
```

## Result Paths

- raw perf output: `results/raw/`
- parsed CSV: `results/processed/perf_results.csv`
- plots: `plots/`
- report figures: `report/figures/`

## Build Entry Points

- benchmark build: `make`
- perf collection: `scripts/collect_perf.sh`
- parsing: `scripts/parse_perf.py`
- plotting: `scripts/plot_results.py`

## Reproducibility Notes

- Prefer fixed core pinning with `taskset`
- Record compiler version and kernel version
- Keep the machine idle during runs
- Document unsupported perf events instead of failing the whole sweep
