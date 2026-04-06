# lmbench: Portable Tools for Performance Analysis

- Read depth: Deep
- Category: profiling methods
- Main use in this project: microbenchmark methodology and latency discipline

## One-Line Summary

lmbench is a portable microbenchmark suite for exposing basic performance costs
such as latency, bandwidth, and system-call overhead.

## Problem

How can we measure low-level system performance in a way that stays portable and
repeatable across UNIX-like systems?

## Core Idea

- isolate the operation you want to measure
- keep each test minimal and focused
- use the benchmark suite as a performance-analysis toolkit, not just a score

## Evaluation Context

- general-purpose UNIX/POSIX systems
- designed for portability rather than a single hardware target

## Reproducibility

- Rating: High
- Why: the methodology is benchmark-driven and maps cleanly to small C probes

## Project 1 Connection

- informs `pointer_chase` and `stride_access`
- helps frame how the latency probe should be interpreted

## Extension Question

- which parts of the original suite are still useful today, and which ones need
  modern counter support or better automation?

