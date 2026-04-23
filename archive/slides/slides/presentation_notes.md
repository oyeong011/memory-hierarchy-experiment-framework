# Project 1 Presentation Notes

## Slide 1. Title

- This project asks how simple memory access patterns change performance on Linux.
- The focus is educational and reproducible, not reverse engineering.

## Slide 2. Motivation and Questions

- The main distinction is between latency-bound and bandwidth-bound behavior.
- I wanted a benchmark set small enough to explain but broad enough to show different locality patterns.

## Slide 3. Benchmark Suite and Workflow

- Each benchmark is a single-purpose kernel with a one-line CSV output.
- The workflow is build, collect with `perf`, parse into one CSV, and generate plots.
- On this host the PMU limitation forced a runtime-first interpretation.

## Slide 4. Pointer Chase and STREAM-lite

- Pointer chasing was the strongest result because dependency prevents overlap.
- STREAM-lite shows the opposite case: regular streams keep throughput high for much larger ranges.

## Slide 5. Stride and Traversal Order

- These results were weaker, but still moved in the expected direction.
- The main message is that locality effects can be real without always producing dramatic graphs.

## Slide 6. Tiled Matrix Multiply

- This is the clearest optimization story in the project.
- Larger tiles improved GFLOP/s substantially, which is consistent with better cache reuse.

## Slide 7. Takeaways

- Timing alone can still reveal useful memory behavior.
- The next meaningful upgrade is not a more complex benchmark, but a host with working hardware PMU support.
