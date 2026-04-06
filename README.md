# Memory Hierarchy Experiment Framework

This project provides a reproducible Linux benchmarking workflow for studying cache locality, TLB behavior, and matrix blocking effects with `perf`.

## Korean Description

이 프로젝트는 Linux 환경에서 `perf`를 사용해 캐시 지역성, TLB 동작, 행렬 블로킹 효과를 분석하기 위한 재현 가능한 메모리 계층 벤치마킹 프레임워크입니다. C 벤치마크, Bash 자동화 스크립트, Python 후처리 도구를 포함하며 다양한 메모리 접근 패턴의 성능 변화를 체계적으로 측정할 수 있습니다.

## Directory Structure

```text
/project
  Makefile
  requirements.txt
  /src
    bench_util.h
    pointer_chase.c
    stream_lite.c
    stride_access.c
    row_col_traversal.c
    matmul_tiled.c
  /scripts
    collect_bench_only.sh
    collect_perf.sh
    parse_perf.py
    plot_results.py
    setup_tmux_current_session.sh
    setup_tmux_workspace.sh
  /tests
    smoke_workflow.sh
  /docs
    perf_workflow.md
    reproducibility.md
  /prompts
  /report
    final_report.md
    /figures
  /results
    /raw
    /processed
  /plots
  /wiki
```

## Minimal Memory Pattern Suite

For the current Project 1 workflow, the primary benchmark executables are:

- `bin/pointer_chase`
- `bin/stream_lite`
- `bin/stride_access`
- `bin/row_col_traversal`
- optional: `bin/matmul_tiled`

Build them with:

```bash
make
```

Collect raw `perf` results:

```bash
./scripts/collect_perf.sh
```

If hardware PMU events are unavailable on the machine, the collector will keep
running with software events such as `task-clock` and `page-faults`, while
leaving unsupported hardware-counter columns empty.
Each invocation writes into its own run directory under `results/raw/`.

Parse raw logs into a single CSV:

```bash
python3 ./scripts/parse_perf.py \
  --manifest results/raw/<run-id>/manifest.csv \
  --output results/processed/perf_results.csv
```

If `perf` is blocked on the machine, collect benchmark-only runtime data:

```bash
./scripts/collect_bench_only.sh
```

Generate plots:

```bash
python3 -m pip install --user -r requirements.txt
python3 ./scripts/plot_results.py \
  --input results/processed/perf_results.csv \
  --plots-dir plots
```

## Reproducible Workflow

Build the benchmark suite:

```bash
make
```

Run a `perf` sweep into a fresh per-run directory:

```bash
./scripts/collect_perf.sh
```

Parse one manifest into a flat CSV:

```bash
python3 ./scripts/parse_perf.py \
  --manifest results/raw/<run-id>/manifest.csv \
  --output results/processed/perf_results.csv
```

Generate plots and derived metrics:

```bash
python3 ./scripts/plot_results.py \
  --input results/processed/perf_results.csv \
  --plots-dir plots \
  --derived-output results/processed/perf_results_derived.csv
```

If hardware counters are unavailable on the current machine, fall back to:

```bash
./scripts/collect_bench_only.sh
```

For reproducibility, also see [`docs/reproducibility.md`](docs/reproducibility.md).

- Prefer one fresh run directory per sweep instead of reusing a shared manifest.
- Keep the machine idle during runs.
- Fix CPU frequency if possible and avoid background jobs.
- Use the same compiler and flags across runs.
- Repeat each sweep on the same kernel and hardware configuration when comparing datasets.
- Ensure `perf` is usable for the current user. On many Ubuntu systems you may need to lower `kernel.perf_event_paranoid`, for example `sudo sysctl kernel.perf_event_paranoid=1`.

## Smoke Test

Run the lightweight verification script when you want a fast check that the
benchmarks, parser, and plotting code still work together:

```bash
./tests/smoke_workflow.sh
```

The smoke test rebuilds the binaries, runs a tiny case for each benchmark,
validates the one-line CSV output, parses a sample `perf` manifest, and
renders plots from the checked-in sample results.

## perf Events Collected

The automation script probes event support and then records the subset available
on the current machine.

Preferred hardware events:

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

Software fallback events:

- `task-clock`
- `cpu-clock`
- `page-faults`
- `context-switches`
- `cpu-migrations`

The parser stores them in structured CSV files under `results/processed/`.

## Interpretation Guide

### Locality Effects

Sequential access benefits from spatial locality because adjacent cache lines are consumed after each fetch. Strided access degrades locality as the stride grows and eventually wastes most of each fetched cache line. Random access removes predictable locality almost entirely and should show higher miss rates and lower IPC once the working set exceeds cache capacity.

### Cache Hierarchy Limits

When runtime or cache miss rate changes sharply as array size increases, that usually marks a working-set transition across L1, L2, LLC, or DRAM capacity. The size-vs-time curve is most useful when you look for inflection points rather than only absolute runtime.

### TLB Behavior

The current minimal suite does not include a dedicated page-walk benchmark, so
TLB interpretation is opportunistic rather than central. If `dTLB-load-misses`
is available, wider strides and larger working sets can still hint at
translation pressure, but those signals should be described conservatively.

### Blocking Effectiveness

In the optional `matmul_tiled` benchmark, blocking keeps submatrices hot in
cache for longer than an untiled traversal. The best tile size is usually the
one that improves reuse without paying too much loop overhead, so the primary
signal is the tile-size versus performance curve rather than one absolute
"best" number.

## CV Description

Built an automated Linux benchmarking framework in C, Bash, and Python to study memory hierarchy behavior using `perf`. Designed reproducible experiments for sequential, strided, random, matrix-multiplication, and virtual-memory access patterns; automated parameter sweeps up to 1 GB working sets; collected structured hardware-counter data; and produced analysis pipelines for cache miss rate, IPC, TLB behavior, and blocking efficiency.

## Project 2 Wiki

Project 2 starts in `wiki/`:

- `wiki/README.md` for the paper taxonomy and reading order
- `wiki/reproducibility-map.md` for a paper-to-reproduction map
- `wiki/scaled-repro-plan.md` for the 2-week scaled reproduction plan
