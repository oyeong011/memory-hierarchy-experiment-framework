# Hitting the Memory Wall: Implications of the Obvious

- Read depth: Skim
- Category: access patterns
- Main use in this project: conceptual background for cache and DRAM limits

## One-Line Summary

This classic paper explains why memory latency and bandwidth become system-level
bottlenecks as CPUs outpace the memory hierarchy.

## Problem

Why does improving the CPU alone stop helping once memory becomes the limiting
resource?

## Core Idea

- caches hide some latency but not all of it
- access patterns matter
- bandwidth and latency limits are architectural, not just software problems

## Evaluation Context

- broad architectural discussion
- used mainly for intuition and framing

## Reproducibility

- Rating: High for the idea, not for the original platform numbers

## Project 1 Connection

- explains the staircase behavior in `pointer_chase`
- motivates the row-major versus column-major comparison

## Extension Question

- what is the smallest set of local experiments needed to make the memory-wall
  argument visible on this machine?

