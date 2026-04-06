# Memory Systems Paper Wiki

This wiki is a starter structure for Project 2. It is organized so that a
single undergraduate can keep the reading list, reproduction notes, and paper
cards in one place without overfitting the scope.

## Reading Taxonomy

- access patterns
- profiling methods
- workload-DRAM interaction
- memory tiering / CXL

## Reading Order

### Deep Read

- [STREAM: Sustainable Memory Bandwidth in High Performance Computers](papers/stream.md)
- [lmbench: Portable Tools for Performance Analysis](papers/lmbench.md)
- [Telescope: Profiling Memory Access Patterns at the Terabyte-scale](papers/telescope.md)
- [TPP: Transparent Page Placement for CXL-Enabled Tiered-Memory](papers/tpp.md)

### Skim Read

- [nanoBench: A Low-Overhead Tool for Running Microbenchmarks on x86 Systems](papers/nanobench.md)
- [Hitting the Memory Wall: Implications of the Obvious](papers/hitting-memory-wall.md)
- [Overcoming the Memory Wall with CXL-Enabled SSDs](papers/overcoming-memory-wall-cxl-ssd.md)
- [Managing Memory Tiers with CXL in Virtualized Environments](papers/managing-memory-tiers-cxl.md)

## Folder Structure

```text
wiki/
  README.md
  reproducibility-map.md
  scaled-repro-plan.md
  papers/
    stream.md
    lmbench.md
    telescope.md
    tpp.md
    nanobench.md
    hitting-memory-wall.md
    overcoming-memory-wall-cxl-ssd.md
    managing-memory-tiers-cxl.md
```

## Notes

- Keep each card short enough to scan in one screen.
- Record exact bib details later only when you actually need them for the report.
- Use the project 1 benchmarks as the local baseline for claims about latency,
  bandwidth, and locality.
