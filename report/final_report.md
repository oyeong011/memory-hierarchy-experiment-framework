# Linux `perf` Based Memory Access Pattern and System Level Memory Bottleneck Analysis

## Abstract

This project studies how simple memory access patterns change execution cost on
Linux systems. The benchmark suite covers dependent pointer chasing,
STREAM-inspired sequential bandwidth kernels, strided array traversal,
row-major versus column-major traversal, and an optional tiled matrix
multiplication kernel. The workflow combines C microbenchmarks, `perf stat`
automation, CSV parsing, and Python plotting.

The main goal is not to reverse engineer hidden hardware details, but to show
how software-visible timing and counter trends reflect locality. On the current
host, hardware PMU events such as `cycles`, `instructions`, and `cache-misses`
were unavailable even after permissions were opened, so the final analysis is
driven primarily by runtime measurements plus software events. Even with that
limitation, the dataset shows three clear patterns: pointer chasing becomes much
slower as the working set grows, sequential STREAM-lite kernels sustain much
higher throughput than irregular access, and tiled matrix multiplication
improves performance by increasing reuse. The weaker cases, such as stride and
row/column traversal, still show locality-sensitive trends, but they are less
cleanly isolated on this machine.

## 1. Introduction

Modern processors can retire arithmetic operations far faster than they can
fetch data from the full memory hierarchy. As a result, the layout and ordering
of memory accesses often matter as much as the arithmetic itself. A loop that
touches nearby elements sequentially may run near a stable bandwidth plateau,
while a loop with dependency chains or poor locality may stall on latency long
before the arithmetic becomes expensive.

This project examines that gap through a small set of microbenchmarks. The
target is not production realism; the target is interpretability. Each
benchmark isolates one access pattern strongly enough that the resulting timing
curve can be connected to locality. The project also emphasizes reproducibility:
each benchmark emits a single CSV row, `perf` runs are collected into
per-invocation manifests, and the analysis pipeline is scripted end to end.

The report focuses on three questions.

1. How does runtime change as the working set grows for different memory access patterns?
2. Which observations remain reliable when hardware PMU counters are unavailable?
3. What simple software-level changes appear beneficial from the measured data?

## 2. Background

### 2.1 Locality and the Cache Hierarchy

Temporal locality means recently used data is likely to be reused soon.
Spatial locality means nearby data is likely to be used together. Caches exploit
both assumptions. Sequential array traversal usually benefits from spatial
locality because one cache-line fill supplies multiple nearby elements. Large
strides or irregular traversals reduce that benefit by using only a fraction of
each fetched line.

### 2.2 Latency-Bound and Bandwidth-Bound Kernels

Not all memory-intensive kernels fail for the same reason. A dependent pointer
chase is latency bound because each load depends on the previous load, which
prevents deep overlap. In contrast, a regular streaming loop can expose enough
parallelism for the memory system to approach a bandwidth-limited steady state.
Comparing these cases helps separate "slow because each access stalls" from
"slow because total bytes moved are large."

### 2.3 `perf stat` as Supporting Evidence

`perf stat` can provide aggregate signals such as cycles, instructions, cache
misses, and TLB-related events. These are useful for interpretation, but not
all machines expose the same events, and event availability depends on kernel
permissions and PMU support. For that reason, this project treats counters as
supporting evidence rather than as complete ground truth.

## 3. Methodology

### 3.1 Experimental Environment

The measurements were collected on a Linux host with the following observable
properties.

- Kernel: `Linux 5.15.0-173-generic`
- Architecture: `aarch64`
- CPU vendor/model string exposed by the system: Apple
- Core count: 4 cores, 1 thread per core
- Compiler model: `cc -O2 -std=c11 -Wall -Wextra -Wpedantic`

The collector pinned workloads to a fixed core with `taskset`, performed a
warmup pass before timed runs, and repeated each configuration multiple times.
The machine was treated as a shared workstation rather than as a perfectly
isolated lab system, so all conclusions are phrased conservatively.

### 3.2 Benchmark Suite

The benchmark suite consists of five small kernels.

- `pointer_chase`: dependent loads through a randomized cyclic structure
- `stream_lite`: sequential copy and triad kernels
- `stride_access`: one-dimensional traversal with variable stride
- `row_col_traversal`: row-major versus column-major traversal of a row-major matrix
- `matmul_tiled` (optional): blocked matrix multiplication with configurable tile size

Each program accepts its problem size from the command line and prints exactly
one CSV row:

`benchmark,variant,size_bytes,elements,rows,cols,stride,tile_size,elapsed_sec,extra`

The `extra` field is benchmark-specific. For example, it stores ns/load for
pointer chasing, GB/s for STREAM-lite, ns/access for stride traversal, and
GFLOP/s for tiled matrix multiply.

### 3.3 Counter Collection Strategy

The preferred event set was:

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

The collector probes event support before the sweep starts. Unsupported events
are skipped explicitly rather than silently ignored.

On this host, `perf_event_paranoid` could be lowered successfully, but the
hardware PMU events above still appeared as unsupported. As a result, the
practical dataset for this report contains software events such as
`task-clock`, `cpu-clock`, `page-faults`, `context-switches`, and
`cpu-migrations`, while the hardware-counter columns remain empty. This is the
main limitation of the current study.

### 3.4 Data and Artifact Pipeline

The pipeline is:

1. Build benchmarks with `make`.
2. Run `./scripts/collect_perf.sh` into a fresh `results/raw/<run-id>/` directory.
3. Parse one manifest into a flat CSV with `scripts/parse_perf.py`.
4. Generate derived metrics and plots with `scripts/plot_results.py`.

The checked-in aggregate dataset used for this report is:

- Parsed CSV: `results/processed/perf_results_with_matmul.csv`
- Derived CSV: `results/processed/perf_results_with_matmul_derived.csv`
- Figures: `report/figures/`

## 4. Results

### 4.1 Pointer Chasing

Pointer chasing produced the clearest result in the entire suite. As the
working set increased, the average dependent-load latency rose sharply:

- 512 KiB: about 20.2 ns/load
- 8 MiB: about 110.8 ns/load
- 32 MiB: about 313.4 ns/load

The corresponding mean elapsed time also grew dramatically, from about 42 ms at
512 KiB to about 3.72 s at 8 MiB and about 42.1 s at 32 MiB. This is exactly
the kind of workload where poor locality cannot be hidden by overlap, so the
staircase-like slowdown is consistent with progressively more expensive cache
and memory accesses.

Related figure: [latency_staircase.png](/home/oy/Project/report/figures/latency_staircase.png)

### 4.2 STREAM-lite

The sequential STREAM-lite kernels behaved much more smoothly. For the smallest
and medium working sets, the measured throughput stayed on a clear bandwidth
plateau.

- Copy: about 32.2 GB/s at 96 KiB, 31.5 GB/s at 384 KiB, 33.2 GB/s at 1.5 MiB
- Triad: about 48.1 GB/s at 96 KiB, 49.6 GB/s at 384 KiB, 46.2 GB/s at 1.5 MiB

At larger sizes the throughput dropped, though not monotonically:

- Copy: about 9.38 GB/s at 6 MiB, then 23.2 GB/s at 24 MiB and 21.3 GB/s at 96 MiB
- Triad: about 16.2 GB/s at 6 MiB, then 30.8 GB/s at 24 MiB and 25.2 GB/s at 96 MiB

The main takeaway is not one exact peak number, but that regular streaming
access sustains much higher useful throughput than irregular patterns. Even
without hardware counters, the contrast with pointer chasing is clear.

Related figure: [bandwidth_vs_working_set.png](/home/oy/Project/report/figures/bandwidth_vs_working_set.png)

### 4.3 Stride Access

The stride benchmark showed locality-sensitive timing, but the result was less
clean than a textbook miss-rate curve. For small working sets, increasing the
stride changed access cost only modestly:

- 32 KiB: about 0.52 ns/access at stride 1 and about 0.90 ns/access at stride 256

For larger working sets, the effect became stronger:

- 512 KiB: about 0.60 ns/access at stride 1 and about 1.26 ns/access at stride 256
- 8 MiB: about 0.77 ns/access at stride 1 and about 1.29 ns/access at stride 256
- 32 MiB: about 0.70 ns/access at stride 1, about 4.62 ns/access at stride 16, and about 6.06 ns/access at stride 256

Because hardware cache-miss counters were unavailable, this figure should be
interpreted as a timing-based locality signal rather than as a measured miss
curve. The result is still useful: wider spacing generally made accesses more
expensive once the footprint became large enough.

Related figure: [stride_access_cost_vs_stride.png](/home/oy/Project/report/figures/stride_access_cost_vs_stride.png)

### 4.4 Row-Major vs Column-Major Traversal

The row-major versus column-major experiment showed the expected direction of
effect. The difference was small at the smallest matrix, then grew with size.

- 256x256: row about 1.256 ns/element, column about 1.287 ns/element
- 512x512: row about 1.288 ns/element, column about 1.780 ns/element
- 1024x1024: row about 1.765 ns/element, column about 2.832 ns/element

This is consistent with row-major traversal matching the underlying storage
layout better than column-major traversal. The effect is weaker than the
pointer-chase result, but still visible and useful as a software-level locality
example.

Related figure: [row_vs_column_traversal.png](/home/oy/Project/report/figures/row_vs_column_traversal.png)

### 4.5 Tiled Matrix Multiply

The optional tiled matrix-multiply benchmark produced the clearest software
optimization result in the project. Increasing the tile size improved measured
performance for both tested matrix sizes.

- 128x128: about 2.17 GFLOP/s at tile 8, 2.97 at tile 16, 3.63 at tile 32, and 3.80 at tile 64
- 256x256: about 2.20 GFLOP/s at tile 8, 2.94 at tile 16, 3.63 at tile 32, and 3.85 at tile 64

This is strong evidence that making reuse explicit through blocking can improve
performance even in a small educational benchmark. Unlike the stride case, the
trend here is both large and consistent.

Related figures:

- [tile_size_effect.png](/home/oy/Project/report/figures/tile_size_effect.png)
- [roofline_draft.png](/home/oy/Project/report/figures/roofline_draft.png)

### 4.6 Software Event Observations

The software counters tracked timing in a broadly sensible way. `task-clock`
and `cpu-clock` followed elapsed time closely in the smoke and full sweeps.
`context-switches` and `cpu-migrations` remained low, which suggests that the
benchmarks were not heavily perturbed by scheduler noise. These signals do not
replace hardware PMU counters, but they do help confirm that the process was
not being interrupted constantly.

## 5. Discussion

Three conclusions are defensible from the current dataset.

First, dependent-load latency is extremely sensitive to working-set growth. The
pointer-chasing benchmark slowed down by more than an order of magnitude as the
working set moved beyond small-cache scales. This was the strongest result in
the suite and the least ambiguous one.

Second, regular streaming and blocked reuse remain effective software-level
patterns. STREAM-lite maintained far higher useful throughput than irregular
access, and `matmul_tiled` showed a large gain from increasing the tile size.
These are practical reminders that locality-friendly code often matters even
before considering more advanced optimizations.

Third, the weaker benchmarks still matter. The stride and row/column cases were
not dramatic, but they did move in the expected direction. That is a useful
lesson by itself: some locality effects are obvious only when the footprint is
large enough or when the machine exposes richer counter support.

## 6. Limitations

This study was run on a single Linux machine, so the results are specific to
one hardware and kernel configuration. The largest limitation is counter
availability. Although `perf` itself was usable, the preferred hardware PMU
events were unsupported on this host. That means the report cannot make claims
that depend on direct measurements of cycles, instructions, or cache misses.

The benchmark suite is also intentionally synthetic. That is useful for
interpretability, but it does not represent the full behavior of production
applications. Finally, several effects can overlap in any timing curve:
cache-capacity transitions, prefetch behavior, translation overhead, and general
system noise cannot be fully separated without richer hardware support.

## 7. Conclusion

The project demonstrates that a small, reproducible benchmark suite can still
show meaningful memory-system behavior on commodity Linux systems. The clearest
results were the pointer-chasing latency staircase, the steady throughput of
sequential STREAM-lite kernels, and the performance gain from tiled matrix
multiplication. The stride and traversal benchmarks were weaker, but still
showed locality-sensitive timing trends.

The main methodological lesson is equally important: runtime-only analysis can
still be useful, but it must be described carefully when hardware PMU support is
missing. For this reason, the most defensible claims in the report are about
timing trends and software-visible locality rather than about exact cache-level
miss attribution.

## 8. Future Work

The next practical extension is to rerun the same suite on a Linux host that
exposes hardware PMU counters. That would allow direct analysis of miss rates,
IPC, and cache-level trends that are currently unavailable. A second extension
is to broaden the benchmark set with page-size experiments, NUMA-aware
placement, or a dedicated TLB-stress benchmark. A third extension is to turn
the current roofline draft into a more systematic performance model using a
larger floating-point kernel set.

## 9. Reproducibility Notes

The repository already contains the full workflow:

- Benchmarks: `src/`
- Collectors: `scripts/collect_perf.sh`, `scripts/collect_bench_only.sh`
- Parser: `scripts/parse_perf.py`
- Plotting: `scripts/plot_results.py`
- Smoke test: `tests/smoke_workflow.sh`
- Reproducibility guide: `docs/reproducibility.md`

Current artifact locations used by this report:

- Raw manifests: `results/raw/run_*/manifest.csv`
- Parsed perf CSV: `results/processed/perf_results_with_matmul.csv`
- Derived metrics CSV: `results/processed/perf_results_with_matmul_derived.csv`
- Plot directory: `plots/`
- Report figures: `report/figures/`
