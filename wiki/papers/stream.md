# STREAM: Sustainable Memory Bandwidth in High Performance Computers

- Read depth: Deep
- Category: access patterns
- Main use in this project: baseline bandwidth curve and locality story

## One-Line Summary

STREAM is the canonical memory bandwidth benchmark: it measures how much
sustained bandwidth a machine can deliver on simple streaming kernels.

## Problem

How do we measure sustainable memory bandwidth in a way that is simple,
portable, and comparable across systems?

## Core Idea

- use very simple streaming kernels
- keep the code easy to reason about
- separate bandwidth measurement from higher-level application noise

## Evaluation Context

- classic HPC-style memory bandwidth benchmarking
- paper focuses on sustained throughput, not just peak frequency behavior

## Reproducibility

- Rating: High
- Why: the kernel shape is simple, and the local `stream_lite` benchmark already
  captures the main behavior

## Project 1 Connection

- directly overlaps with `stream_lite`
- supports the bandwidth-vs-working-set figure in the report

## Extension Question

- how much of the observed curve is cache hierarchy behavior versus DRAM
  bandwidth saturation?

