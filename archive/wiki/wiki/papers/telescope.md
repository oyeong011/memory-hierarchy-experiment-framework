# Telescope: Profiling Memory Access Patterns at the Terabyte-scale

- Read depth: Deep
- Category: profiling methods
- Main use in this project: motivates access-pattern telemetry at scale

## One-Line Summary

Telescope proposes a low-overhead way to profile memory access patterns at very
large working-set sizes using page-table structure.

## Problem

How do we track hot and cold memory behavior when the working set is too large
for traditional high-frequency sampling to be practical?

## Core Idea

- use page-table structure to infer access patterns
- keep telemetry accurate at large scale
- reduce overhead compared with heavy sampling schemes

## Evaluation Context

- terabyte-scale workloads
- tiered and disaggregated memory settings

## Reproducibility

- Rating: Medium
- Why: the overall design can be summarized locally, but the original scale and
  kernel integration are not practical on this machine

## Project 1 Connection

- adds vocabulary for interpreting locality beyond raw runtime plots
- connects to `perf` counters, working-set sweeps, and future tiering work

## Extension Question

- can a simplified local setup approximate the same hot/cold page story using
  only counters and synthetic sweeps?

