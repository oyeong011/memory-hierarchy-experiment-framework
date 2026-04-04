#!/usr/bin/env python3
from __future__ import annotations

import argparse
import csv
import math
import statistics
from collections import defaultdict
from pathlib import Path
from typing import Dict, Iterable, List

try:
    import matplotlib.pyplot as plt
except ImportError:  # pragma: no cover
    plt = None


def load_csv(path: Path) -> List[Dict[str, str]]:
    with path.open(newline="") as handle:
        return list(csv.DictReader(handle))


def to_float(value: str) -> float | None:
    if value is None or value == "":
        return None
    try:
        return float(value)
    except ValueError:
        return None


def add_derived_metrics(rows: Iterable[Dict[str, str]]) -> List[Dict[str, object]]:
    enriched: List[Dict[str, object]] = []
    for row in rows:
        entry: Dict[str, object] = dict(row)
        cycles = to_float(row.get("cycles", ""))
        instructions = to_float(row.get("instructions", ""))
        cache_refs = to_float(row.get("cache_references", ""))
        cache_misses = to_float(row.get("cache_misses", ""))
        time_seconds = to_float(row.get("time_seconds", ""))

        entry["cycles_f"] = cycles
        entry["instructions_f"] = instructions
        entry["cache_references_f"] = cache_refs
        entry["cache_misses_f"] = cache_misses
        entry["time_seconds_f"] = time_seconds
        entry["ipc"] = (instructions / cycles) if cycles and instructions is not None and cycles > 0 else None
        entry["cache_miss_rate"] = (
            cache_misses / cache_refs if cache_refs and cache_misses is not None and cache_refs > 0 else None
        )
        enriched.append(entry)
    return enriched


def group_mean(rows: Iterable[Dict[str, object]], key_fields: List[str], value_field: str) -> List[Dict[str, object]]:
    grouped: Dict[tuple, List[float]] = defaultdict(list)
    for row in rows:
        value = row.get(value_field)
        if value is None:
            continue
        grouped[tuple(row[field] for field in key_fields)].append(float(value))

    summary: List[Dict[str, object]] = []
    for key, values in sorted(grouped.items()):
        item: Dict[str, object] = {field: key[idx] for idx, field in enumerate(key_fields)}
        item[f"mean_{value_field}"] = statistics.mean(values)
        summary.append(item)
    return summary


def print_table(title: str, rows: List[Dict[str, object]], columns: List[str]) -> None:
    print(f"\n{title}")
    if not rows:
        print("  no rows")
        return

    widths = {
        column: max(len(column), *(len(f"{row.get(column, '')}") for row in rows))
        for column in columns
    }
    print("  " + "  ".join(column.ljust(widths[column]) for column in columns))
    print("  " + "  ".join("-" * widths[column] for column in columns))
    for row in rows:
        print("  " + "  ".join(f"{row.get(column, '')}".ljust(widths[column]) for column in columns))


def ensure_parent(path: Path) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)


def save_summary_csv(path: Path, rows: List[Dict[str, object]]) -> None:
    if not rows:
        return
    ensure_parent(path)
    fieldnames = list(rows[0].keys())
    with path.open("w", newline="") as handle:
        writer = csv.DictWriter(handle, fieldnames=fieldnames)
        writer.writeheader()
        writer.writerows(rows)


def plot_memory_time(rows: List[Dict[str, object]], out_path: Path) -> None:
    if plt is None:
        return
    data = [row for row in rows if row.get("mode") in {"seq", "rand"}]
    grouped: Dict[str, List[tuple[float, float]]] = defaultdict(list)
    for row in data:
        size_bytes = to_float(str(row.get("array_size_bytes", "")))
        time_seconds = row.get("time_seconds_f")
        if size_bytes is None or time_seconds is None:
            continue
        grouped[str(row["mode"])].append((size_bytes / (1024.0 * 1024.0), float(time_seconds)))

    plt.figure(figsize=(8, 5))
    for mode, points in sorted(grouped.items()):
        points.sort()
        plt.plot([x for x, _ in points], [y for _, y in points], marker="o", label=mode)
    plt.xlabel("Array size (MiB)")
    plt.ylabel("Time (s)")
    plt.title("Memory Access: Size vs Time")
    plt.xscale("log", base=2)
    plt.grid(True, which="both", linestyle=":")
    plt.legend()
    ensure_parent(out_path)
    plt.tight_layout()
    plt.savefig(out_path)
    plt.close()


def plot_stride_miss_rate(rows: List[Dict[str, object]], out_path: Path) -> None:
    if plt is None:
        return
    data = [row for row in rows if row.get("mode") == "stride"]
    grouped: Dict[str, List[tuple[float, float]]] = defaultdict(list)
    for row in data:
        stride = to_float(str(row.get("stride_elements", "")))
        miss_rate = row.get("cache_miss_rate")
        size_bytes = to_float(str(row.get("array_size_bytes", "")))
        if stride is None or miss_rate is None or size_bytes is None:
            continue
        label = f"{int(size_bytes / (1024.0 * 1024.0))} MiB"
        grouped[label].append((stride, float(miss_rate)))

    plt.figure(figsize=(8, 5))
    for label, points in sorted(grouped.items(), key=lambda item: float(item[1][0][0]) if item[1] else math.inf):
        points.sort()
        plt.plot([x for x, _ in points], [y for _, y in points], marker="o", label=label)
    plt.xlabel("Stride (elements)")
    plt.ylabel("Cache miss rate")
    plt.title("Strided Access: Stride vs Cache Miss Rate")
    plt.xscale("log", base=4)
    plt.grid(True, which="both", linestyle=":")
    plt.legend()
    ensure_parent(out_path)
    plt.tight_layout()
    plt.savefig(out_path)
    plt.close()


def plot_block_performance(rows: List[Dict[str, object]], out_path: Path) -> None:
    if plt is None:
        return
    data = [row for row in rows if row.get("variant") == "blocked"]
    grouped: Dict[str, List[tuple[float, float]]] = defaultdict(list)
    for row in data:
        n = row.get("n")
        block_size = to_float(str(row.get("block_size", "")))
        time_seconds = row.get("time_seconds_f")
        if n is None or block_size is None or time_seconds is None:
            continue
        grouped[str(n)].append((block_size, float(time_seconds)))

    plt.figure(figsize=(8, 5))
    for n, points in sorted(grouped.items(), key=lambda item: int(item[0])):
        points.sort()
        plt.plot([x for x, _ in points], [y for _, y in points], marker="o", label=f"n={n}")
    plt.xlabel("Block size")
    plt.ylabel("Time (s)")
    plt.title("Blocked Matrix Multiplication: Block Size vs Time")
    plt.grid(True, linestyle=":")
    plt.legend()
    ensure_parent(out_path)
    plt.tight_layout()
    plt.savefig(out_path)
    plt.close()


def main() -> None:
    parser = argparse.ArgumentParser(description="Post-process benchmark CSV results.")
    parser.add_argument("--results-dir", default="results", help="Directory containing benchmark CSV files")
    parser.add_argument("--plots-dir", default="plots", help="Directory to write PNG plots")
    args = parser.parse_args()

    results_dir = Path(args.results_dir)
    plots_dir = Path(args.plots_dir)

    memory_rows = add_derived_metrics(load_csv(results_dir / "memory_access.csv"))
    matrix_rows = add_derived_metrics(load_csv(results_dir / "matrix.csv"))
    vm_rows = add_derived_metrics(load_csv(results_dir / "virtual_memory.csv"))

    memory_summary = group_mean(memory_rows, ["mode", "array_size_bytes"], "time_seconds_f")
    memory_ipc = group_mean(memory_rows, ["mode", "array_size_bytes"], "ipc")
    stride_summary = group_mean(memory_rows, ["array_size_bytes", "stride_elements"], "cache_miss_rate")
    matrix_summary = group_mean(matrix_rows, ["variant", "n", "block_size"], "time_seconds_f")
    vm_summary = group_mean(vm_rows, ["mode", "bytes"], "cache_miss_rate")

    print_table("Memory Time Summary", memory_summary, ["mode", "array_size_bytes", "mean_time_seconds_f"])
    print_table("Memory IPC Summary", memory_ipc, ["mode", "array_size_bytes", "mean_ipc"])
    print_table("Stride Miss Rate Summary", stride_summary, ["array_size_bytes", "stride_elements", "mean_cache_miss_rate"])
    print_table("Matrix Time Summary", matrix_summary, ["variant", "n", "block_size", "mean_time_seconds_f"])
    print_table("Virtual Memory Miss Rate Summary", vm_summary, ["mode", "bytes", "mean_cache_miss_rate"])

    save_summary_csv(results_dir / "summary_memory_time.csv", memory_summary)
    save_summary_csv(results_dir / "summary_memory_ipc.csv", memory_ipc)
    save_summary_csv(results_dir / "summary_stride_miss_rate.csv", stride_summary)
    save_summary_csv(results_dir / "summary_matrix_time.csv", matrix_summary)
    save_summary_csv(results_dir / "summary_vm_miss_rate.csv", vm_summary)

    if plt is None:
        print("\nmatplotlib is not installed; skipping plot generation.")
    else:
        plot_memory_time(memory_rows, plots_dir / "memory_size_vs_time.png")
        plot_stride_miss_rate(memory_rows, plots_dir / "stride_vs_miss_rate.png")
        plot_block_performance(matrix_rows, plots_dir / "block_size_vs_performance.png")
        print(f"\nPlots written to {plots_dir}")


if __name__ == "__main__":
    main()
