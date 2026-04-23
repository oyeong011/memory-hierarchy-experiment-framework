# Reproducibility Map

This map connects the paper wiki to what Project 1 can already reproduce, what
is only partially reproducible on this host, and what should stay as reading-only
background.

| Paper | Category | What to reproduce | Project 1 overlap | Feasibility |
| --- | --- | --- | --- | --- |
| STREAM: Sustainable Memory Bandwidth in High Performance Computers | access patterns | Bandwidth vs working-set shape and copy/triad behavior | `stream_lite`, bandwidth plots | High |
| lmbench: Portable Tools for Performance Analysis | profiling methods | Latency-style microbenchmarks and measurement discipline | `pointer_chase`, `stride_access` | High |
| Telescope: Profiling Memory Access Patterns at the Terabyte-scale | profiling methods | How access hotness and region-level telemetry are described | `perf` workflow, working-set sweeps | Medium |
| TPP: Transparent Page Placement for CXL-Enabled Tiered-Memory | memory tiering / CXL | Hot/cold page placement logic and tiering tradeoffs | working-set transitions, locality discussion | Low on this host |
| nanoBench: A Low-Overhead Tool for Running Microbenchmarks on x86 Systems | profiling methods | Microbenchmark methodology and low-noise runs | benchmark harness design | Medium |
| Hitting the Memory Wall: Implications of the Obvious | access patterns | The conceptual ceiling story behind cache and DRAM limits | staircase plots, stride curves | High |
| Overcoming the Memory Wall with CXL-Enabled SSDs | workload-DRAM interaction | Memory-wall arguments for CXL-backed slower tiers | discussion section only | Low on this host |
| Managing Memory Tiers with CXL in Virtualized Environments | memory tiering / CXL | Tiering policy under virtualization and CXL constraints | discussion and future work | Low on this host |

## Practical Reading Rule

- Reproduce the shape first, not the exact numeric values.
- If a paper depends on specialized hardware, capture the design and evaluation
  logic instead of forcing a fake local reimplementation.
- Use Project 1 results as the bridge between classic microbenchmarks and the
  paper claims.
