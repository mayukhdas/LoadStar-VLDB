#!/usr/bin/env python3
"""
Download testbed Excel parts (1-29) from Zenodo and combine into a single CSV.
"""

import os
import zipfile
from pathlib import Path
from concurrent.futures import ProcessPoolExecutor
import requests
import pandas as pd

BASE_URL = "https://zenodo.org/records/16539984/files"
OUTPUT_FILE = "testbed.csv"
DOWNLOAD_DIR = Path("testbed_parts")

COL_NAMES = [
    'Timestamp', 'PartitionId', 'UniqueId', 'ReplicaId',
    'sum_TotalRequestCharge', 'MaxCounterCPU',
    'MaxCounterValueMemory', 'TotalUsageInMB',
]
# Primary key for deduplication (one row per key in final CSV)
DEDUPE_KEY_COLS = ['Timestamp', 'PartitionId', 'UniqueId']

EXCEL_ENGINE = "calamine"


def is_valid_xlsx(path: Path) -> bool:
    try:
        zipfile.ZipFile(path, 'r').close()
        return True
    except (zipfile.BadZipFile, Exception):
        return False


def download_part(part_num: int, force: bool = False) -> Path:
    """Download a single part file; returns path to saved file."""
    # filename = f"testbed_10march_16march_part{part_num}.xlsx"
    filename = f"testbed_3march_9march_part{part_num}.xlsx"
    url = f"{BASE_URL}/{filename}"
    path = DOWNLOAD_DIR / filename
    path.parent.mkdir(parents=True, exist_ok=True)

    if path.exists() and not force:
        if is_valid_xlsx(path):
            print(f"Using cached part {part_num}/29: {filename}")
            return path
        print(f"Cached part {part_num}/29 is corrupt, re-downloading ...")

    print(f"Downloading part {part_num}/29: {filename} ...", end=" ", flush=True)
    r = requests.get(url, stream=True, timeout=120)
    r.raise_for_status()
    with open(path, "wb") as f:
        for chunk in r.iter_content(chunk_size=8192):
            f.write(chunk)
    print("OK")

    if not is_valid_xlsx(path):
        raise RuntimeError(f"Downloaded {filename} is not a valid xlsx file")
    return path


def _read_one_xlsx(path: Path) -> pd.DataFrame:
    """Read a single xlsx part using the fast calamine engine."""
    return pd.read_excel(
        path, header=None, names=COL_NAMES, engine=EXCEL_ENGINE,
    )


def combine_excel_files(paths: list[Path], max_workers: int = 4) -> None:
    """Read xlsx parts in parallel and stream directly to a single CSV."""
    first = True
    with ProcessPoolExecutor(max_workers=max_workers) as pool:
        for i, df in enumerate(pool.map(_read_one_xlsx, paths), 1):
            print(f"  Read part {i}/{len(paths)}: {len(df):,} rows")
            df.to_csv(
                OUTPUT_FILE, mode='w' if first else 'a',
                header=first, index=False,
            )
            first = False
            del df


def main():
    parts = list(range(1, 30))
    paths = []

    for part_num in parts:
        path = download_part(part_num)
        paths.append(path)

    print(f"\nCombining {len(paths)} files ...")
    combine_excel_files(paths)

    row_count_before = sum(1 for _ in open(OUTPUT_FILE)) - 1
    print(f"Combined: {OUTPUT_FILE} ({row_count_before:,} rows)")

    # Remove duplicate rows on (Timestamp, PartitionId, UniqueId), keep first
    print(f"Removing duplicate keys {DEDUPE_KEY_COLS} ...")
    df = pd.read_csv(OUTPUT_FILE, header=0)
    df.columns = [c.strip() for c in df.columns]
    n_before = len(df)
    df = df.drop_duplicates(subset=DEDUPE_KEY_COLS, keep='first')
    n_after = len(df)
    df.to_csv(OUTPUT_FILE, index=False)
    removed = n_before - n_after
    if removed:
        print(f"  Removed {removed:,} duplicate rows. Saved: {OUTPUT_FILE} ({n_after:,} rows)")
    else:
        print(f"  No duplicates. Saved: {OUTPUT_FILE} ({n_after:,} rows)")


if __name__ == "__main__":
    main()
