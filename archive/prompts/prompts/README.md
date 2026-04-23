# Memory Project Prompt Bundle

This directory contains ready-to-paste prompts and operating notes for the
multi-session workflow of the memory systems project.

## Session Map

- `session_a_benchmark.md`: benchmark implementation
- `session_b_perf.md`: perf automation and CSV collection
- `session_c_plot.md`: parsing, metrics, and plotting
- `session_d_report.md`: technical report drafting
- `session_e_wiki.md`: paper wiki construction
- `session_f_scaled_repro.md`: reduced-scope paper reproduction planning
- `shared_conventions.md`: common file names, CLI, and CSV expectations

## Recommended Order

1. Read `shared_conventions.md`
2. Start Session A and Session B in parallel
3. Start Session C after the CSV schema is fixed
4. Start Session D once the benchmark list and experiment methodology are fixed
5. Run Session E and Session F independently from Project 1

## tmux Windows

The workspace script creates the following windows:

- `00-hub`
- `A-bench`
- `B-perf`
- `C-plot`
- `D-report`
- `E-wiki`
- `F-repro`
