# Managing Memory Tiers with CXL in Virtualized Environments

- Read depth: Skim
- Category: memory tiering / CXL
- Main use in this project: future-work reference for virtualization-aware tiering

## One-Line Summary

This paper studies how to manage CXL-based memory tiers when the system is
running under virtualization.

## Problem

How do we make CXL tiering work when software-based placement has overhead and
hardware-based placement is not fully aware of application behavior?

## Core Idea

- combine hardware-managed tiering with software-managed isolation
- address overheads that show up in virtualized cloud settings
- keep the memory system useful under real operational constraints

## Evaluation Context

- cloud and virtualized environments
- CXL memory tiers

## Reproducibility

- Rating: Low on this host
- Why: it depends on a deployment context far beyond the current local machine

## Project 1 Connection

- good future-work framing for the final report
- helps explain why project 1 stays at the benchmark and counter level

## Extension Question

- can a small local experiment at least mimic the policy tradeoffs even if the
  actual CXL setup is unavailable?

