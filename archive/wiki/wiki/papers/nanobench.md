# nanoBench: A Low-Overhead Tool for Running Microbenchmarks on x86 Systems

- Read depth: Skim
- Category: profiling methods
- Main use in this project: measurement discipline for tiny kernels

## One-Line Summary

nanoBench focuses on running small microbenchmarks with low overhead and
careful control over the measurement environment.

## Problem

How do we measure very small code fragments without letting the measurement
framework dominate the result?

## Core Idea

- minimize benchmark noise
- keep the runtime overhead low
- make counter collection usable for tiny kernels

## Evaluation Context

- x86 systems
- microarchitectural benchmarking and counter-driven analysis

## Reproducibility

- Rating: Medium
- Why: the methodology is transferable, even if the exact low-level setup is not
  the same as this project

## Project 1 Connection

- useful for validating that the local harness does not distort tiny runs
- relevant for `pointer_chase` and `stride_access` at small sizes

## Extension Question

- which parts of the nanoBench discipline can be adopted in a Bash/Python
  workflow without adding much complexity?

