#!/usr/bin/env python3
from __future__ import annotations

import argparse
import csv
from pathlib import Path
from typing import Dict


EXPECTED_EVENT_MAP = {
    "cycles": "cycles",
    "instructions": "instructions",
    "cache-references": "cache_references",
    "cache-misses": "cache_misses",
    "branch-misses": "branch_misses",
    "L1-dcache-loads": "l1_dcache_loads",
    "L1-dcache-load-misses": "l1_dcache_load_misses",
    "LLC-loads": "llc_loads",
    "LLC-load-misses": "llc_load_misses",
    "dTLB-load-misses": "dtlb_load_misses",
    "task-clock": "task_clock_msec",
    "cpu-clock": "cpu_clock_msec",
    "page-faults": "page_faults",
    "context-switches": "context_switches",
    "cpu-migrations": "cpu_migrations",
}

OUTPUT_COLUMNS = [
    "run_id",
    "benchmark",
    "variant",
    "size_bytes",
    "elements",
    "rows",
    "cols",
    "stride",
    "tile_size",
    "repeat_idx",
    "core_id",
    "elapsed_sec",
    "extra",
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
    "perf_repeat",
    "stdout_path",
    "perf_path",
    "status",
]


def load_manifest(path: Path) -> list[dict[str, str]]:
    with path.open(newline="") as handle:
        return list(csv.DictReader(handle))


def parse_count(raw: str) -> str:
    value = raw.strip().replace(",", "")
    if not value or value.startswith("<"):
        return ""
    if "+-" in value:
        value = value.split("+-", 1)[0].strip()
    return value


def normalize_event(event_name: str) -> str:
    name = event_name.strip()
    if ":" in name:
        name = name.split(":", 1)[0]
    return name


def parse_perf_file(path: Path) -> Dict[str, str]:
    counters = {column: "" for column in EXPECTED_EVENT_MAP.values()}
    if not path.exists():
        return counters

    for line in path.read_text().splitlines():
        if not line.strip():
            continue
        fields = [field.strip() for field in line.split(";")]
        if len(fields) < 3:
            continue
        event_name = normalize_event(fields[2])
        if event_name not in EXPECTED_EVENT_MAP:
            continue
        counters[EXPECTED_EVENT_MAP[event_name]] = parse_count(fields[0])

    return counters


def build_row(manifest_row: dict[str, str]) -> dict[str, str]:
    perf_path = Path(manifest_row["perf_path"])
    counters = parse_perf_file(perf_path)

    row = {
        "run_id": manifest_row["run_id"],
        "benchmark": manifest_row["benchmark"],
        "variant": manifest_row["variant"],
        "size_bytes": manifest_row["size_bytes"],
        "elements": manifest_row["elements"],
        "rows": manifest_row["rows"],
        "cols": manifest_row["cols"],
        "stride": manifest_row["stride"],
        "tile_size": manifest_row["tile_size"],
        "repeat_idx": manifest_row["repeat_idx"],
        "core_id": manifest_row["core_id"],
        "elapsed_sec": manifest_row["elapsed_sec"],
        "extra": manifest_row["extra"],
        "perf_repeat": manifest_row["perf_repeat"],
        "stdout_path": manifest_row["stdout_path"],
        "perf_path": manifest_row["perf_path"],
        "status": manifest_row["status"],
    }
    row.update(counters)
    return row


def main() -> None:
    parser = argparse.ArgumentParser(description="Parse perf stat raw logs into a benchmark CSV table.")
    parser.add_argument(
        "--manifest",
        required=True,
        help="Path to a per-run manifest, for example results/raw/<run-id>/manifest.csv",
    )
    parser.add_argument("--output", required=True, help="Output CSV path")
    args = parser.parse_args()

    manifest_path = Path(args.manifest)
    output_path = Path(args.output)
    output_path.parent.mkdir(parents=True, exist_ok=True)

    manifest_rows = load_manifest(manifest_path)
    output_rows = [build_row(row) for row in manifest_rows]

    with output_path.open("w", newline="") as handle:
        writer = csv.DictWriter(handle, fieldnames=OUTPUT_COLUMNS)
        writer.writeheader()
        writer.writerows(output_rows)

    print(f"Wrote {len(output_rows)} rows to {output_path}")


if __name__ == "__main__":
    main()
