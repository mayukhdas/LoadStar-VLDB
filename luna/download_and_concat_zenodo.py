#!/usr/bin/env python3
"""
Download all 29 parts of testbed_10march_16march from Zenodo and concatenate into one file.
Also writes min_max_timestamps.csv (UTF-8) for use by Preliminary Processor/generate_input_file.py.
Usage: python download_and_concat_zenodo.py [--output FILE] [--format xlsx|csv]
"""

import argparse
import io
import sys
from pathlib import Path
from urllib.request import urlopen, Request
from urllib.error import URLError, HTTPError

import pandas as pd

BASE_URL = "https://zenodo.org/records/16539984/files/testbed_10march_16march_part{n}.xlsx"
NUM_PARTS = 29
EXCEL_MAX_ROWS = 1_048_576  # Excel .xlsx sheet row limit
UTF8 = "utf-8"


def download_part(part_num: int) -> bytes:
    """Download a single part and return its raw bytes."""
    url = BASE_URL.format(n=part_num)
    # Zenodo often serves the file with ?download=1 for direct download
    for try_url in (f"{url}?download=1", url):
        req = Request(try_url, headers={"User-Agent": "Mozilla/5.0 (compatible; ZenodoDownload/1.0)"})
        try:
            with urlopen(req, timeout=120) as resp:
                return resp.read()
        except (URLError, HTTPError):
            continue
    raise URLError(f"Failed to download part {part_num} from {url}")


def _infer_min_max_columns(df: pd.DataFrame):
    """Return (part_col, replica_col, role_col, ts_col) for min/max aggregation, or None if not found."""
    cols_lower = {c.lower(): c for c in df.columns}
    part = cols_lower.get("partitionid") or next((cols_lower[k] for k in cols_lower if "partition" in k), None)
    replica = cols_lower.get("replicaid") or next((cols_lower[k] for k in cols_lower if "replica" in k), None)
    role = cols_lower.get("roleinstance") or next(
        (cols_lower[k] for k in cols_lower if "role" in k and "instance" in k), None
    )
    ts = (
        cols_lower.get("timestamp")
        or cols_lower.get("precisetimestamp")
        or next((cols_lower[k] for k in cols_lower if "timestamp" in k), None)
    )
    if part and replica and role and ts:
        return (part, replica, role, ts)
    return None


def _write_min_max_timestamps(combined: pd.DataFrame, out_dir: Path) -> None:
    """Derive min/max timestamps per (PartitionId, ReplicaId, RoleInstance) and write UTF-8 CSV."""
    names = _infer_min_max_columns(combined)
    if not names:
        print("Skipping min_max_timestamps.csv: could not find PartitionId/ReplicaId/RoleInstance/timestamp columns.", file=sys.stderr)
        return
    part_col, replica_col, role_col, ts_col = names
    agg = (
        combined.groupby([part_col, replica_col, role_col], as_index=False)[ts_col]
        .agg(min_TIMESTAMP="min", max_TIMESTAMP="max")
    )
    agg.columns = ["PartitionId", "ReplicaId", "RoleInstance", "min_TIMESTAMP", "max_TIMESTAMP"]
    path = out_dir / "min_max_timestamps.csv"
    agg.to_csv(path, index=False, header=False, encoding=UTF8)
    print(f"Wrote {len(agg):,} rows to {path} (UTF-8)", file=sys.stderr)


def main():
    parser = argparse.ArgumentParser(description="Download and concat Zenodo testbed parts 1–29")
    parser.add_argument(
        "--output", "-o",
        default="testbed_10march_16march_full.xlsx",
        help="Output filename (default: testbed_10march_16march_full.xlsx)",
    )
    parser.add_argument(
        "--format", "-f",
        choices=("xlsx", "csv"),
        default="xlsx",
        help="Output format (default: xlsx)",
    )
    parser.add_argument(
        "--no-download",
        action="store_true",
        help="Skip download; only concat already-downloaded part*.xlsx in current dir",
    )
    parser.add_argument(
        "--min-max-output",
        type=Path,
        default=None,
        help="Directory or path for min_max_timestamps.csv (default: same dir as --output)",
    )
    args = parser.parse_args()

    try:
        import openpyxl  # noqa: F401
    except ImportError:
        print("Reading .xlsx requires openpyxl. Install with: pip install openpyxl", file=sys.stderr)
        sys.exit(1)

    out_path = Path(args.output)
    if args.format == "csv" and out_path.suffix.lower() != ".csv":
        out_path = out_path.with_suffix(".csv")

    frames = []
    for n in range(1, NUM_PARTS + 1):
        if args.no_download:
            part_path = Path(f"testbed_10march_16march_part{n}.xlsx")
            if not part_path.exists():
                print(f"Part file not found: {part_path}", file=sys.stderr)
                sys.exit(1)
            df = pd.read_excel(part_path, engine="openpyxl")
        else:
            print(f"Downloading part {n}/{NUM_PARTS}...", flush=True)
            raw = download_part(n)
            df = pd.read_excel(io.BytesIO(raw), engine="openpyxl")
        frames.append(df)

    print("Concatenating...", flush=True)
    combined = pd.concat(frames, axis=0, ignore_index=True)

    use_csv = args.format == "csv"
    if not use_csv and len(combined) > EXCEL_MAX_ROWS:
        use_csv = True
        out_path = out_path.with_suffix(".csv")
        print(
            f"Data has {len(combined):,} rows; Excel max is {EXCEL_MAX_ROWS:,}. Saving as CSV instead.",
            file=sys.stderr,
        )

    if use_csv:
        combined.to_csv(out_path, index=False, encoding=UTF8)
    else:
        combined.to_excel(out_path, index=False, engine="openpyxl")

    print(f"Saved {len(combined):,} rows to {out_path}")

    # Write min_max_timestamps.csv (UTF-8) for Preliminary Processor/generate_input_file.py
    min_max_dir = args.min_max_output if args.min_max_output is not None else out_path.parent
    if min_max_dir.suffix:
        min_max_dir = min_max_dir.parent
    _write_min_max_timestamps(combined, min_max_dir)


if __name__ == "__main__":
    main()
