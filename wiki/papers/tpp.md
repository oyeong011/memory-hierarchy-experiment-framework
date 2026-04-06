# TPP: Transparent Page Placement for CXL-Enabled Tiered-Memory

- Read depth: Deep
- Category: memory tiering / CXL
- Main use in this project: tiering policy and hot/cold placement concept

## One-Line Summary

TPP studies how to place pages across local DRAM and CXL-backed memory while
keeping the system transparent to applications.

## Problem

How can a tiered memory system keep hot pages close to the CPU without putting
too much migration or sampling overhead on the kernel?

## Core Idea

- track hot and cold pages
- promote critical data to fast memory
- demote colder data to slower memory
- keep the policy application-transparent

## Evaluation Context

- CXL-enabled memory tiering
- production-oriented server workloads

## Reproducibility

- Rating: Medium
- Why: the policy logic is understandable, but the hardware and kernel setup are
  not available on this host

## Project 1 Connection

- helps interpret working-set transitions in the latency and bandwidth curves
- gives a concrete future-work direction for tiered memory

## Extension Question

- which parts of the policy can be approximated with software-only hotness
  heuristics in a normal Linux setup?

