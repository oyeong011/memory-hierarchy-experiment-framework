# Parallel Session Operating Table

| Session | Goal | Inputs | Outputs | Done Criteria |
|---|---|---|---|---|
| A | Benchmark implementation | `prompts/shared_conventions.md`, `prompts/session_a_benchmark.md` | `src/*.c`, `Makefile`, `bin/*` | Four benchmarks build and run with CSV stdout |
| B | perf automation | `prompts/shared_conventions.md`, `prompts/session_b_perf.md`, Session A binaries | `scripts/collect_perf.sh`, `scripts/parse_perf.py`, raw perf CSV | Perf sweep runs, unsupported events are logged, parsed CSV is emitted |
| C | Parsing and plotting | `prompts/shared_conventions.md`, `prompts/session_c_plot.md`, Session B parsed CSV | `scripts/plot_results.py`, `plots/*.png` | Plots render even with partial data |
| D | Report drafting | `prompts/session_d_report.md`, method details from Sessions A and B, plots from Session C | `report/` draft files | Report outline and draft sections are ready |
| E | Paper wiki | `prompts/session_e_wiki.md` | `wiki/` or `notes/` structure proposal, paper cards, README | Eight-paper taxonomy and notes are ready |
| F | Reduced-scope reproduction planning | `prompts/session_f_scaled_repro.md`, Project 1 scope | candidate comparison and 2-week reproduction plan | One recommended paper and a scoped plan are ready |

## Merge Order

1. Session A
2. Session B
3. Session C
4. Session D
5. Session E and Session F independently

## High-Risk Merge Points

- benchmark binary names
- benchmark CLI argument order
- benchmark stdout CSV column order
- parsed perf CSV schema
- plot input file path assumptions
