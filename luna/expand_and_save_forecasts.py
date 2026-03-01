import pandas as pd
from pathlib import Path
import numpy as np
import re

OUTPUTS_DIR = Path(__file__).resolve().parent.parent / "outputs"
PREDICTIONS_FILENAME = "predictions_by_timestamp_partition.csv"
PRED_LOAD_PATTERN = re.compile(r"^pred_load_(\d+)$")


def expand_forecast(df: pd.DataFrame, value_col: str) -> pd.DataFrame:
    """Expand 2-hour block predictions to 5-minute intervals. Uses value_col (e.g. pred_load_50)."""
    df = df.copy()
    df["TIMESTAMP"] = pd.to_datetime(df["TIMESTAMP"])
    df["TIMESTAMP"] = df["TIMESTAMP"] + pd.Timedelta(days=1)
    df = df[["TIMESTAMP", "PartitionId", value_col]]
    df = df.groupby(["TIMESTAMP", "PartitionId"]).agg({value_col: "max"}).reset_index()

    new_rows = []
    for partition_id in df["PartitionId"].unique():
        partition_data = df[df["PartitionId"] == partition_id]
        for i in range(len(partition_data)):
            current_row = partition_data.iloc[i]
            current_timestamp = current_row["TIMESTAMP"]

            if current_timestamp.minute == 45:
                new_rows.append(current_row)
                next_timestamp = current_timestamp + pd.Timedelta(minutes=5)
                for _ in range(5):
                    new_row = current_row.copy()
                    new_row["TIMESTAMP"] = next_timestamp
                    new_rows.append(new_row)
                    next_timestamp += pd.Timedelta(minutes=5)
            elif current_timestamp.minute == 15:
                new_rows.append(current_row)
                next_timestamp = current_timestamp + pd.Timedelta(minutes=5)
                for _ in range(5):
                    new_row = current_row.copy()
                    new_row["TIMESTAMP"] = next_timestamp
                    new_rows.append(new_row)
                    next_timestamp += pd.Timedelta(minutes=5)
            else:
                new_rows.append(current_row)

    expanded = pd.DataFrame(new_rows)
    expanded["TIMESTAMP"] = pd.to_datetime(expanded["TIMESTAMP"]).dt.tz_localize("UTC")
    expanded.iloc[:, 2] = expanded.iloc[:, 2].astype(int)
    return expanded


def quantile_to_filename_affix(quantile_int: int) -> str:
    """Map pred_load_N column index N to forecast filename suffix, e.g. 90 -> '0.9', 1 -> '0.01'."""
    if quantile_int == 0:
        return "0.001"
    if quantile_int == 99:
        return "0.99"
    s = f"{quantile_int / 100:.2f}".rstrip("0").rstrip(".")
    return s or "0"


def main():
    if not OUTPUTS_DIR.is_dir():
        raise SystemExit(f"Outputs directory not found: {OUTPUTS_DIR}")

    prediction_files = sorted(OUTPUTS_DIR.glob(f"*/{PREDICTIONS_FILENAME}"))
    if not prediction_files:
        raise SystemExit(f"No {PREDICTIONS_FILENAME} found under {OUTPUTS_DIR}")

    for path in prediction_files:
        subdir_name = path.parent.name
        df = pd.read_csv(path)
        pred_cols = [c for c in df.columns if PRED_LOAD_PATTERN.match(c)]
        if not pred_cols:
            print(f"  No pred_load_* columns in {path}, skipping")
            continue
        print(f"Processing {subdir_name} ({len(pred_cols)} quantiles) ...")
        for col in sorted(pred_cols):
            m = PRED_LOAD_PATTERN.match(col)
            n = int(m.group(1))
            suffix = quantile_to_filename_affix(n)
            out_name = f"forecast_{suffix}.csv"
            expanded = expand_forecast(df, col)
            out_path = path.parent / out_name
            expanded.to_csv(out_path, header=False, index=False)
            print(f"  -> {out_path}")
    print("Done.")


if __name__ == "__main__":
    main()
