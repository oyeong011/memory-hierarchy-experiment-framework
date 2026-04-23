# Overcoming the Memory Wall with CXL-Enabled SSDs

- Read depth: Skim
- Category: workload-DRAM interaction
- Main use in this project: motivation for slower tiers and memory expansion

## One-Line Summary

This paper explores using CXL-attached flash as a way to address the memory
wall with caching and prefetching support.

## Problem

Can a slower memory-like tier still be useful if the system can hide some of the
latency and lifetime costs with smart policies?

## Core Idea

- add a CXL-backed slower tier
- use caching and prefetching to reduce the visible penalty
- analyze the tradeoff between cost, performance, and lifetime

## Evaluation Context

- real-world application traces
- CXL-enabled flash device design space

## Reproducibility

- Rating: Low on this host
- Why: the central system relies on CXL-backed hardware behavior that is not
  available locally

## Project 1 Connection

- useful as a discussion anchor when explaining why a simple local benchmark
  does not cover tiered-memory systems

## Extension Question

- what software-only parts of the argument can still be illustrated with a
  local working-set sweep?

