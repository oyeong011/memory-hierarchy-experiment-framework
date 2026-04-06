# Scaled Reproduction Plan: STREAM

## Why This Paper

STREAM is the best fit for a scaled reproduction because Project 1 already has a
close local implementation in `stream_lite`. The paper is also easy to explain:
measure sustainable memory bandwidth under simple streaming kernels and compare
the shape of the results across working-set sizes.

## Goal

Reproduce the core idea of STREAM at undergraduate scale:

- show how memory bandwidth changes as the working set moves past cache sizes
- compare at least two kernels, such as copy and triad
- keep the implementation and analysis simple enough to finish in two weeks

## What To Reproduce

- working-set sweep from cache-sized arrays to DRAM-sized arrays
- runtime and derived bandwidth for each kernel
- one figure that looks like a bandwidth-vs-size curve
- a short note on why the curve changes near cache boundaries

## What Not To Attempt

- exact hardware matching with the original paper
- multi-node or multi-socket scaling claims
- exhaustive kernel coverage beyond the simple kernels already in Project 1
- any claim that requires the original 1990s platform

## Deliverables

- a small C benchmark or a thin wrapper around the existing `stream_lite`
- one CSV with the sweep results
- one plot for bandwidth vs working set size
- one short markdown note comparing the shape of the result with Project 1

## Two-Week Schedule

### Week 1

- read the paper and pin down the exact claim to reproduce
- confirm the benchmark interface and sweep sizes
- run a few tiny cases to make sure the CSV format is stable
- capture a baseline figure from the local machine

### Week 2

- run the full sweep with repeated trials
- compute summary statistics and generate the final plot
- write a short reproducibility note
- compare the result against Project 1 `stream_lite`

## Success Criteria

- the curve is monotonic enough to explain cache and DRAM transitions
- the benchmark runs on the current machine without special hardware
- the result is short, defensible, and reproducible from the repository

