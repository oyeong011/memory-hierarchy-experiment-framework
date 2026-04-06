# Linux perf Based Memory Access Pattern and System Level Memory Bottleneck Analysis

## 1. Outline

1. Introduction
2. Background
3. Methodology
4. Results
5. Discussion
6. Limitations
7. Future Work
8. Reproducibility Appendix

## 2. Chapter Checklist

### Introduction

- Why memory behavior still dominates performance in simple loops and matrix kernels
- Why locality-sensitive microbenchmarks are useful for education and systems analysis
- Scope of this project and what is intentionally excluded

### Background

- Cache hierarchy and locality
- Latency-bound versus bandwidth-bound access patterns
- What `perf stat` can and cannot tell us
- Why TLB and cache effects can appear together in the same benchmark

### Methodology

- Linux environment, compiler, CPU model, and kernel version
- Benchmark list and rationale
- Input size sweeps and traversal parameters
- `perf stat` event list and unsupported-event handling
- Controlled conditions: core pinning, warmup, idle system, frequency notes

### Results

- Pointer chasing latency staircase
- STREAM-lite bandwidth scaling
- Stride-dependent miss behavior
- Row-major versus column-major traversal comparison
- Optional tiled matrix multiplication section

### Discussion

- Where locality helped most clearly
- When the observed counter changes matched runtime changes
- Where counter interpretation remained ambiguous
- What software-level changes appear most effective

### Limitations

- Single-machine study
- Restricted access to some hardware counters on locked-down systems
- No direct visibility into DRAM internals
- Small benchmark suite compared with production workloads

### Future Work

- Add tiled matrix multiplication and a simple roofline estimate
- Compare multiple CPUs or cache hierarchies
- Add page-size experiments or NUMA-aware variants

## 3. Introduction Draft

Modern processors can execute arithmetic instructions much faster than data can
be delivered from the full memory hierarchy. As a result, the access pattern of
otherwise simple code often determines whether execution is limited by cache
latency, memory bandwidth, or address translation overhead. This project studies
that effect using a small set of reproducible microbenchmarks and Linux
`perf stat` counter collection.

The goal is not to claim direct measurement of internal DRAM organization or to
reverse engineer proprietary hardware behavior. Instead, the project focuses on
software-visible symptoms of locality and memory pressure. Four benchmark
families are used: pointer chasing, STREAM-lite bandwidth kernels, strided
array traversal, and row-major versus column-major matrix traversal. Together,
these cases cover dependent loads, sequential streaming, reduced spatial
locality, and layout-sensitive two-dimensional access.

This report aims to answer three practical questions. First, how do different
memory access patterns change observed runtime as the working set grows? Second,
which hardware counter trends are most useful when explaining those changes?
Third, what simple code transformations appear beneficial at the software level?
The resulting analysis is intended to be realistic for an undergraduate systems
project and to remain reproducible on commodity Linux machines.

## 4. Background Draft

### 4.1 Cache Hierarchy and Locality

Temporal locality means recently used data is likely to be used again soon.
Spatial locality means nearby data is likely to be used together. Processor
caches exploit both assumptions. Sequential array traversal benefits from
spatial locality because a fetched cache line supplies several neighboring
elements. In contrast, large-stride or random traversal may use only a small
fraction of each fetched line, increasing miss pressure and wasting bandwidth.

### 4.2 Latency Bound and Bandwidth Bound Behavior

Not all memory-intensive kernels are limited by the same bottleneck. A dependent
pointer chase is primarily latency bound because each load depends on the
previous result, which limits overlap across outstanding misses. STREAM-style
loops are closer to bandwidth bound because they expose long regular streams and
allow substantial overlap in the memory subsystem. Comparing these cases helps
separate "slow because each access stalls" from "slow because total bytes moved
are large."

### 4.3 perf Counter Interpretation

`perf stat` reports aggregate hardware and software counters such as cycles,
instructions, cache references, and cache misses. These counters are useful for
high-level interpretation, but they are not perfect ground truth. Event
availability depends on kernel permissions and PMU support, and a reported cache
miss metric does not by itself identify which cache level or structure caused
the dominant slowdown. For that reason, this project uses counters as supporting
evidence alongside controlled benchmark design and runtime measurements.

### 4.4 TLB and Address Translation Effects

Memory performance is affected not only by cache fills but also by virtual to
physical address translation. When a benchmark touches many distinct pages with
limited locality, translation overhead can rise together with cache miss
pressure. In this project, TLB-related counters are treated as a complementary
signal rather than a standalone explanation unless the benchmark structure makes
page footprint a clear first-order factor.

## 5. Methodology Draft

### 5.1 Experimental Environment

This study was run on a Linux host with the following observable properties:

- OS/kernel: `Linux 5.15.0-173-generic`
- Architecture: `aarch64`
- CPU vendor/model: Apple, 4 cores, 1 thread per core
- Compiler: `cc` with `-O2 -std=c11 -Wall -Wextra -Wpedantic`

The machine exposed only limited `perf` support. `perf_event_paranoid` was set to
`0`, but the hardware PMU counters used in the preferred event list were still
reported as unsupported on this host. As a result, the final analysis relies on
runtime measurements plus software events such as `task-clock`, `cpu-clock`,
`page-faults`, `context-switches`, and `cpu-migrations`. Raw runs are stored per
invocation under `results/raw/run_*`, and the parsed dataset is assembled from
the corresponding manifest file.

### 5.2 Benchmarks

The benchmark suite is intentionally small and transparent.

- `pointer_chase`: a random cyclic linked structure with dependent loads to emphasize latency
- `stream_lite`: simple sequential kernels such as copy and triad to study bandwidth scaling
- `stride_access`: one-dimensional traversal with configurable stride to reduce spatial locality
- `row_col_traversal`: comparison of row-major and column-major traversal over the same row-major allocation
- optional `matmul_tiled`: blocked matrix multiplication to study reuse improvements

Each benchmark accepts problem size parameters from the command line and emits a
single CSV line. This makes the suite easy to automate and minimizes ambiguity
in post-processing. The optional tiled matrix-multiply benchmark was included in
the current run set and is reported separately below.

### 5.3 perf Events

The preferred event set is:

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

Because some systems do not expose every event to unprivileged users, the
automation script probes support first and skips unsupported events with an
explicit warning. If `perf` access is blocked completely, the run is marked as a
setup limitation rather than silently producing incomplete data.

On this host, the hardware events were unavailable even after permissions were
opened, so the actual dataset contains empty hardware-counter columns and filled
software-counter columns instead. That limitation should be stated directly in
the report rather than hidden.

### 5.4 Controlled Conditions

Describe the controls in practical rather than absolute terms:

- pin the benchmark to a fixed core with `taskset`
- keep the machine otherwise idle
- run a warmup before measurement unless explicitly disabled
- repeat each configuration multiple times
- keep compiler flags and benchmark binaries fixed throughout the experiment
- record the `perf_event_paranoid` value and note whether counters were restricted

This framing is more defensible than claiming perfect isolation on a shared
workstation.

## 6. Results Caption Templates

### 6.1 Pointer Chasing

**Figure 1. Pointer chasing latency staircase.** The pointer-chasing benchmark
shows the clearest size-dependent slowdown in the suite. Average dependent-load
latency rose from about 1.9 ns/load at a 32 KiB working set to about 8.8
ns/load at 512 KiB and about 22 ns/load at 8 MiB. Wall-clock runtime increased
from roughly 0.25 ms to 18-19 ms and then to roughly 0.62-0.87 s across the same
sizes. That pattern is consistent with a latency-bound workload crossing cache
capacity boundaries.

### 6.2 STREAM-lite

**Figure 2. Bandwidth vs working set size.** The STREAM-lite kernels sustained
roughly 30-33 GB/s for copy and roughly 46-49 GB/s for triad at the smallest
sizes, with a modest drop at the largest working set. The copy kernel was more
stable across sizes than the triad kernel, but both remained in a bandwidth
plateau for the small and medium working sets. The runtime trend is useful even
without hardware PMU counters because it shows when a sequential kernel begins
to lose steady throughput.

### 6.3 Stride Access

**Figure 3. Stride access cost versus stride.** The stride benchmark did not
produce a clean textbook staircase on this host, but it did show that runtime
changes with access spacing. For the smaller working sets, the measured access
cost stayed near the sub-nanosecond to low-nanosecond range for most strides,
while the largest working sets showed larger slowdowns at wider strides. Because
the host did not expose the preferred hardware counters, the stride plot should
be read as a timing-based locality signal rather than a precise cache-miss
measurement.

### 6.4 Row-major vs Column-major

**Figure 4. Row-major versus column-major traversal.** The row-major and
column-major runs were close at 256x256, both around 1.2 ms per pass. At
512x512 and 1024x1024, the row-major traversal remained consistently faster, but
the gap was still modest relative to the overall runtime. The expected locality
advantage is visible, but on this host it is smaller than the pointer-chase
effect and should be described as a steady trend rather than an extreme gap.

### 6.5 Software Counters

The software counter columns help frame the measurements but not replace missing
hardware PMU data. `task-clock` and `cpu-clock` track closely with elapsed time,
and `page-faults` are most noticeable during first-touch allocation and
initialization. `context-switches` and `cpu-migrations` remained low in the
captured runs, which suggests that the benchmark process was not heavily
disturbed by scheduling noise.

### 6.6 Optional Tiled Matrix Multiply

**Figure 5. Tile size effect.** The optional tiled matrix-multiply benchmark
shows the clearest benefit from a software-level reuse optimization in the
current dataset. For the 128x128 case, measured throughput rose from about 2.1
GFLOP/s at tile size 8 to about 3.8 GFLOP/s at tile size 64. The 256x256 case
showed the same pattern, with the larger tile sizes outperforming the smallest
tile size by a similar margin. This is a useful contrast to the weaker stride
and traversal results: when the access pattern is made explicitly more
cache-friendly, the improvement is easier to see even without PMU counters.

## 7. Discussion Points

### 7.1 What Was Clear

Pointer chasing was the most convincing benchmark in the suite. Its dependent
load chain created a visible latency staircase, and the increase from a small
working set to an 8 MiB working set was large enough to stand out even with the
limited counter support on this machine. STREAM-lite also behaved as expected in
the broad sense: sequential access produced a stable bandwidth plateau, and the
larger working set showed some loss of throughput.

### 7.2 What Was Less Clear

The stride and row/column benchmarks were weaker than the ideal textbook
version. That does not mean the locality effects are absent; it means the
measurement setup here was not strong enough to isolate them cleanly. The host
did not expose the preferred hardware counters, so cache-miss rate and IPC could
not be used to corroborate the runtime plots. As a result, the stride and
traversal comparisons should be described as suggestive rather than definitive.

### 7.3 Interpretation Limits

The main limitation is attribution. Runtime, software events, and problem size
can tell us that memory behavior matters, but they do not let us claim direct
visibility into specific cache levels. Because the dataset lacks cycles,
instructions, and cache-miss counts, the report should avoid statements such as
"L1 misses caused X%" or "DRAM bandwidth was fully saturated." A safer claim is
that the observed timing trends are consistent with cache-capacity and locality
effects.

### 7.4 Practical Takeaway

For an undergraduate systems project, the most useful software-level lesson is
still the same: make access patterns regular, keep traversal row-major when the
layout is row-major, and prefer kernels that expose reuse. Among the tested
benchmarks, pointer chasing and STREAM-lite gave the strongest evidence for
this, while the 2D traversal benchmark showed that the effect can be smaller
than expected when the working set is modest or the machine is noisy. The tiled
matrix-multiply case provides the most concrete example of a simple
optimization improving throughput in the current run set.

## 8. Limitations Draft

This project studies memory behavior through software-visible counters and
microbenchmarks on a single Linux system. The results therefore reflect one
hardware and kernel configuration rather than a general law across all
processors. Some counters may also be unavailable or permission-restricted,
which can limit the granularity of the analysis. On this host, the preferred
hardware PMU events were not exposed even after `perf_event_paranoid` was set to
`0`, so the report cannot make claims that require hardware counter evidence.
In addition, the benchmarks are simple and intentionally synthetic. They help
isolate locality effects, but they do not represent the full complexity of
production workloads.

Another limitation is interpretability. A cache miss counter is not a complete
explanation by itself, and several effects such as cache capacity, prefetching,
TLB behavior, and scheduling noise can overlap. For that reason, the report
should avoid over-claiming direct observation of internal memory structures.

## 9. Future Work Draft

A natural extension is to add tiled matrix multiplication and compare multiple
tile sizes to study explicit reuse optimization. Another extension is to repeat
the same benchmark suite on different CPUs or cache hierarchies, which would
turn the current single-system case study into a comparative analysis. Further
work could also include page-size experiments, NUMA-aware placement, or a
simple roofline-style summary once floating-point kernels are added in a more
systematic way.

## 10. Reproducibility Appendix Template

- Git commit hash:
- CPU model:
- Kernel version:
- Compiler and flags:
- `perf_event_paranoid` value:
- Governor / turbo state:
- Core pinning policy:
- Raw result directory:
- Parsed CSV path:
- Plot output directory:

### 10.1 Current Artifact Locations

- Raw manifests: `results/raw/run_*/manifest.csv`
- Parsed perf CSV: `results/processed/perf_results_with_matmul.csv`
- Derived metrics CSV: `results/processed/perf_results_with_matmul_derived.csv`
- Plot directory: `plots/`
- Report figures: `report/figures/`
