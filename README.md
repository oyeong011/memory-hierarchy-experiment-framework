# Memory Hierarchy Experiment Framework

This project provides a reproducible Linux benchmarking workflow for studying cache locality, TLB behavior, and matrix blocking effects with `perf`.

## Korean Description

이 프로젝트는 Linux 환경에서 `perf`를 사용해 캐시 지역성, TLB 동작, 행렬 블로킹 효과를 분석하기 위한 재현 가능한 메모리 계층 벤치마킹 프레임워크입니다. C 벤치마크, Bash 자동화 스크립트, Python 후처리 도구를 포함하며 다양한 메모리 접근 패턴의 성능 변화를 체계적으로 측정할 수 있습니다.

## Directory Structure

```text
/project
  /src
    memory_access_bench.c
    matrix_bench.c
    virtual_memory_bench.c
  /scripts
    build_benchmarks.sh
    run_experiments.sh
    process_results.py
  /results
  /plots
```

## Reproducible Workflow

Build:

```bash
./scripts/build_benchmarks.sh
```

Run the full sweep:

```bash
./scripts/run_experiments.sh
```

Post-process CSV files and generate plots:

```bash
python3 ./scripts/process_results.py --results-dir results --plots-dir plots
```

For reproducibility:

- Keep the machine idle during runs.
- Fix CPU frequency if possible and avoid background jobs.
- Use the same compiler and flags across runs.
- Use fixed random seeds already built into the scripts.
- Repeat each sweep on the same kernel and hardware configuration when comparing datasets.
- Ensure `perf` is usable for the current user. On many Ubuntu systems you may need to lower `kernel.perf_event_paranoid`, for example `sudo sysctl kernel.perf_event_paranoid=1`.

## perf Events Collected

The automation script records:

- `cycles`
- `instructions`
- `cache-references`
- `cache-misses`
- `page-faults`
- `dTLB-load-misses`

The parser stores them in structured CSV files under `results/`.

## Interpretation Guide

### Locality Effects

Sequential access benefits from spatial locality because adjacent cache lines are consumed after each fetch. Strided access degrades locality as the stride grows and eventually wastes most of each fetched cache line. Random access removes predictable locality almost entirely and should show higher miss rates and lower IPC once the working set exceeds cache capacity.

### Cache Hierarchy Limits

When runtime or cache miss rate changes sharply as array size increases, that usually marks a working-set transition across L1, L2, LLC, or DRAM capacity. The size-vs-time curve is most useful when you look for inflection points rather than only absolute runtime.

### TLB Behavior

The virtual memory benchmark touches one location per page, so it suppresses cache-line locality and emphasizes translation overhead. Random page order should increase `dTLB-load-misses` versus sequential page order. Larger memory sizes also increase page-walk pressure once the active page footprint exceeds TLB reach.

### Blocking Effectiveness

In matrix multiplication, loop reordering improves reuse by traversing rows of `B` and `C` more cache-friendly than the naive `i-j-k` order. Blocking goes further by keeping submatrices hot in cache. The best block size is usually the one that fits the relevant cache level without causing excessive loop overhead; it should reduce time and often improve IPC.

## CV Description

Built an automated Linux benchmarking framework in C, Bash, and Python to study memory hierarchy behavior using `perf`. Designed reproducible experiments for sequential, strided, random, matrix-multiplication, and virtual-memory access patterns; automated parameter sweeps up to 1 GB working sets; collected structured hardware-counter data; and produced analysis pipelines for cache miss rate, IPC, TLB behavior, and blocking efficiency.
