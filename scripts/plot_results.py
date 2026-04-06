#!/usr/bin/env python3
from __future__ import annotations

import argparse
import sys
from pathlib import Path

try:
    import matplotlib.pyplot as plt
except ImportError as exc:  # pragma: no cover
    plt = None
    MATPLOTLIB_IMPORT_ERROR = exc
else:
    MATPLOTLIB_IMPORT_ERROR = None

try:
    import pandas as pd
except ImportError as exc:  # pragma: no cover
    pd = None
    PANDAS_IMPORT_ERROR = exc
else:
    PANDAS_IMPORT_ERROR = None


NUMERIC_COLUMNS = [
    "size_bytes",
    "elements",
    "rows",
    "cols",
    "stride",
    "tile_size",
    "repeat_idx",
    "core_id",
    "elapsed_sec",
    "cycles",
    "instructions",
    "cache_references",
    "cache_misses",
    "branch_misses",
    "l1_dcache_loads",
    "l1_dcache_load_misses",
    "llc_loads",
    "llc_load_misses",
    "dtlb_load_misses",
    "task_clock_msec",
    "cpu_clock_msec",
    "page_faults",
    "context_switches",
    "cpu_migrations",
]


def load_results(path: Path) -> pd.DataFrame:
    if not path.exists():
        return pd.DataFrame()

    df = pd.read_csv(path)
    for column in NUMERIC_COLUMNS:
        if column in df.columns:
            df[column] = pd.to_numeric(df[column], errors="coerce")

    if "cache_references" in df.columns and "cache_misses" in df.columns:
        df["cache_miss_rate"] = df["cache_misses"] / df["cache_references"]
    else:
        df["cache_miss_rate"] = pd.NA

    if "instructions" in df.columns and "cycles" in df.columns:
        df["ipc"] = df["instructions"] / df["cycles"]
    else:
        df["ipc"] = pd.NA

    if "l1_dcache_loads" in df.columns and "l1_dcache_load_misses" in df.columns:
        df["l1d_miss_rate"] = df["l1_dcache_load_misses"] / df["l1_dcache_loads"]
    else:
        df["l1d_miss_rate"] = pd.NA

    if "llc_loads" in df.columns and "llc_load_misses" in df.columns:
        df["llc_miss_rate"] = df["llc_load_misses"] / df["llc_loads"]
    else:
        df["llc_miss_rate"] = pd.NA

    if "extra" in df.columns:
        df["extra_numeric"] = pd.to_numeric(df["extra"], errors="coerce")
    else:
        df["extra_numeric"] = pd.NA

    if "benchmark" in df.columns:
        matmul_mask = df["benchmark"] == "matmul_tiled"
        df["gflops"] = pd.NA
        df.loc[matmul_mask, "gflops"] = df.loc[matmul_mask, "extra_numeric"]

        if "size_bytes" in df.columns:
            df["arithmetic_intensity"] = pd.NA
            valid_intensity = matmul_mask & df["size_bytes"].notna() & (df["size_bytes"] > 0)
            if valid_intensity.any() and "rows" in df.columns and "cols" in df.columns:
                # Draft intensity estimate: total flops divided by the benchmark's
                # accounted matrix footprint (A, B, and C).
                total_flops = 2.0 * (df.loc[valid_intensity, "rows"] ** 3)
                df.loc[valid_intensity, "arithmetic_intensity"] = (
                    total_flops / df.loc[valid_intensity, "size_bytes"]
                )

    return df


def ensure_dir(path: Path) -> None:
    path.mkdir(parents=True, exist_ok=True)


def plot_latency_staircase(df: pd.DataFrame, plots_dir: Path) -> None:
    data = df[df["benchmark"] == "pointer_chase"].copy()
    if data.empty:
        return

    data["ns_per_load"] = pd.to_numeric(data["extra"], errors="coerce")
    grouped = data.groupby("size_bytes", as_index=False)["ns_per_load"].mean().dropna()
    if grouped.empty:
        return

    plt.figure(figsize=(8, 5))
    plt.plot(grouped["size_bytes"] / 1024.0, grouped["ns_per_load"], marker="o")
    plt.xscale("log", base=2)
    plt.xlabel("Working set size (KiB)")
    plt.ylabel("Average latency (ns/load)")
    plt.title("Memory Latency Staircase")
    plt.grid(True, which="both", linestyle=":")
    plt.tight_layout()
    plt.savefig(plots_dir / "latency_staircase.png")
    plt.close()


def plot_stream_bandwidth(df: pd.DataFrame, plots_dir: Path) -> None:
    data = df[df["benchmark"] == "stream_lite"].copy()
    if data.empty:
        return

    data["bandwidth_gbps"] = pd.to_numeric(data["extra"], errors="coerce")
    plt.figure(figsize=(8, 5))
    for variant, group in data.groupby("variant"):
        grouped = group.groupby("size_bytes", as_index=False)["bandwidth_gbps"].mean().dropna()
        if grouped.empty:
            continue
        plt.plot(
            grouped["size_bytes"] / (1024.0 * 1024.0),
            grouped["bandwidth_gbps"],
            marker="o",
            label=variant,
        )

    plt.xscale("log", base=2)
    plt.xlabel("Working set size (MiB)")
    plt.ylabel("Bandwidth (GB/s)")
    plt.title("Bandwidth vs Working Set Size")
    plt.grid(True, which="both", linestyle=":")
    plt.legend()
    plt.tight_layout()
    plt.savefig(plots_dir / "bandwidth_vs_working_set.png")
    plt.close()


def plot_stride_miss_rate(df: pd.DataFrame, plots_dir: Path) -> None:
    data = df[df["benchmark"] == "stride_access"].copy()
    if data.empty:
        return

    metric = "cache_miss_rate"
    ylabel = "Miss rate"
    title = "Cache Miss Rate vs Stride"
    filename = "cache_miss_rate_vs_stride.png"
    if data[metric].isna().all() and "l1d_miss_rate" in data.columns:
        metric = "l1d_miss_rate"
    if data[metric].isna().all():
        data["stride_access_cost"] = pd.to_numeric(data["extra"], errors="coerce")
        metric = "stride_access_cost"
        ylabel = "Access cost (ns/access)"
        title = "Stride Access Cost vs Stride"
        filename = "stride_access_cost_vs_stride.png"

    plt.figure(figsize=(8, 5))
    for size_bytes, group in data.groupby("size_bytes"):
        grouped = group.groupby("stride", as_index=False)[metric].mean().dropna()
        if grouped.empty:
            continue
        label = f"{int(size_bytes / 1024)} KiB"
        plt.plot(grouped["stride"], grouped[metric], marker="o", label=label)

    plt.xscale("log", base=2)
    plt.xlabel("Stride (elements)")
    plt.ylabel(ylabel)
    plt.title(title)
    plt.grid(True, which="both", linestyle=":")
    plt.legend()
    plt.tight_layout()
    if filename == "stride_access_cost_vs_stride.png":
        stale = plots_dir / "cache_miss_rate_vs_stride.png"
    else:
        stale = plots_dir / "stride_access_cost_vs_stride.png"
    if stale.exists():
        stale.unlink()
    plt.savefig(plots_dir / filename)
    plt.close()


def plot_row_vs_col(df: pd.DataFrame, plots_dir: Path) -> None:
    data = df[df["benchmark"] == "row_col_traversal"].copy()
    if data.empty:
        return

    data["shape"] = data["rows"].astype("Int64").astype(str) + "x" + data["cols"].astype("Int64").astype(str)
    data["ns_per_element"] = pd.to_numeric(data["extra"], errors="coerce")
    pivot = data.pivot_table(index="shape", columns="variant", values="ns_per_element", aggfunc="mean")
    if pivot.empty:
        return

    ax = pivot.plot(kind="bar", figsize=(9, 5))
    ax.set_xlabel("Matrix shape")
    ax.set_ylabel("Traversal cost (ns/element)")
    ax.set_title("Row-major vs Column-major Traversal")
    ax.grid(True, axis="y", linestyle=":")
    plt.tight_layout()
    plt.savefig(plots_dir / "row_vs_column_traversal.png")
    plt.close()


def plot_tile_size_effect(df: pd.DataFrame, plots_dir: Path) -> None:
    data = df[df["benchmark"] == "matmul_tiled"].copy()
    if data.empty:
        return

    metric = "elapsed_sec"
    ylabel = "Elapsed time (s)"
    title = "Tile Size Effect"
    if "gflops" in data.columns and not data["gflops"].isna().all():
        metric = "gflops"
        ylabel = "Performance (GFLOP/s)"
        title = "Tile Size Effect on Matmul Performance"

    plt.figure(figsize=(8, 5))
    grouped = data.groupby("tile_size", as_index=False)[metric].mean().dropna()
    if grouped.empty:
        plt.close()
        return
    plt.plot(grouped["tile_size"], grouped[metric], marker="o")
    plt.xlabel("Tile size")
    plt.ylabel(ylabel)
    plt.title(title)
    plt.grid(True, linestyle=":")
    plt.tight_layout()
    plt.savefig(plots_dir / "tile_size_effect.png")
    plt.close()


def plot_roofline_draft(df: pd.DataFrame, plots_dir: Path) -> None:
    data = df[df["benchmark"] == "matmul_tiled"].copy()
    if data.empty or "gflops" not in data.columns or "arithmetic_intensity" not in data.columns:
        return

    plt.figure(figsize=(8, 5))
    plt.scatter(data["arithmetic_intensity"], data["gflops"])
    plt.xscale("log", base=2)
    plt.xlabel("Arithmetic intensity (FLOP/byte)")
    plt.ylabel("Performance (GFLOP/s)")
    plt.title("Roofline Draft")
    plt.grid(True, which="both", linestyle=":")
    plt.tight_layout()
    plt.savefig(plots_dir / "roofline_draft.png")
    plt.close()


def export_derived_metrics(df: pd.DataFrame, output_path: Path) -> None:
    if df.empty:
        return
    output_path.parent.mkdir(parents=True, exist_ok=True)
    df.to_csv(output_path, index=False)


def main() -> None:
    parser = argparse.ArgumentParser(description="Plot perf-based memory benchmark results.")
    parser.add_argument("--input", default="results/processed/perf_results.csv", help="Input CSV from parse_perf.py")
    parser.add_argument("--plots-dir", default="plots", help="Directory for PNG plots")
    parser.add_argument(
        "--derived-output",
        default="results/processed/perf_results_derived.csv",
        help="Path to write derived metrics CSV",
    )
    args = parser.parse_args()

    input_path = Path(args.input)
    plots_dir = Path(args.plots_dir)
    derived_output = Path(args.derived_output)

    if pd is None:
        print(f"pandas is required: {PANDAS_IMPORT_ERROR}", file=sys.stderr)
        raise SystemExit(1)
    if plt is None:
        print(f"matplotlib is required: {MATPLOTLIB_IMPORT_ERROR}", file=sys.stderr)
        raise SystemExit(1)

    ensure_dir(plots_dir)
    df = load_results(input_path)
    if df.empty:
        print(f"No data found at {input_path}; nothing to plot.")
        return

    export_derived_metrics(df, derived_output)
    plot_latency_staircase(df, plots_dir)
    plot_stream_bandwidth(df, plots_dir)
    plot_stride_miss_rate(df, plots_dir)
    plot_row_vs_col(df, plots_dir)
    plot_tile_size_effect(df, plots_dir)
    plot_roofline_draft(df, plots_dir)
    print(f"Plots written to {plots_dir}")


if __name__ == "__main__":
    main()
