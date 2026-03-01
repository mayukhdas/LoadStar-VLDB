"""
Multi-dimension quantile forecasting on testbed data.

Pipeline (inverse of expand_and_save_forecasts.py):
  1. Load testbed xlsx (5-min granularity)
  2. Group by (PartitionId, Timestamp) -> max of value columns
  3. Convert to 2-hour time blocks starting at 00:30
  4. For each of the 4 dimensions, in parallel:
       - Create rolling / time features
       - Create next-day target
       - Train quantile LightGBM models
       - Generate multi-day predictions
"""

import pandas as pd
import numpy as np
import lightgbm as lgb
from sklearn.model_selection import RandomizedSearchCV
from scipy.stats import uniform, randint
from datetime import timedelta
from sklearn.metrics import make_scorer
import joblib
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
import warnings
import os
from concurrent.futures import ProcessPoolExecutor
import time
import psutil
import gc
import argparse

VALUE_COLUMNS = [
    'sum_TotalRequestCharge',
    'MaxCounterCPU',
    'MaxCounterValueMemory',
    'TotalUsageInMB',
]

warnings.filterwarnings('ignore')


def log_memory_usage(message):
    rss = psutil.Process(os.getpid()).memory_info().rss / (1024 * 1024)
    print(f"{message}: {rss:.2f} MB")


# ---------------------------------------------------------------------------
# Loss
# ---------------------------------------------------------------------------

def pinball_loss(y_true, y_pred, quantile):
    error = y_true - y_pred
    return float(np.mean(np.where(error > 0, quantile * error, (quantile - 1) * error)))


# ---------------------------------------------------------------------------
# Timestamp helpers
# ---------------------------------------------------------------------------

def timestamp_to_2hour_block(ts):
    """Map a timestamp to the start of its 2-hour block (blocks begin at :30).

    Blocks: 00:30, 02:30, 04:30, ..., 22:30.
    A timestamp at e.g. 01:15 falls into the 00:30 block;
    a timestamp at 00:10 falls into the previous day's 22:30 block.
    Preserves timezone information.
    """
    shifted = ts - timedelta(minutes=30)
    block_hour = (shifted.hour // 2) * 2
    block_start = shifted.replace(hour=block_hour, minute=0, second=0, microsecond=0)
    result = block_start + timedelta(minutes=30)
    if hasattr(ts, 'tzinfo') and ts.tzinfo is not None and result.tzinfo is None:
        result = result.tz_localize(ts.tzinfo)
    return result


# ---------------------------------------------------------------------------
# Data loading / preprocessing (shared across dimensions)
# ---------------------------------------------------------------------------

def preprocess_data(input_file):
    """Load, deduplicate, and aggregate into 2-hour blocks."""
    col_names = [
        'Timestamp', 'PartitionId', 'UniqueId', 'ReplicaId',
        'sum_TotalRequestCharge', 'MaxCounterCPU',
        'MaxCounterValueMemory', 'TotalUsageInMB',
    ]
    print(f"Loading data from {input_file} ...")
    if input_file.endswith(('.xlsx', '.xls')):
        df = pd.read_excel(input_file, header=None, names=col_names, engine="openpyxl")
    else:
        # CSV is assumed to have a header row (e.g. Timestamp, PartitionId, ...)
        df = pd.read_csv(input_file, header=0)
        df.columns = [c.strip() for c in df.columns]
    log_memory_usage("After reading input file")

    df['Timestamp'] = pd.to_datetime(df['Timestamp'], utc=True)

    present_value_cols = [c for c in VALUE_COLUMNS if c in df.columns]
    if not present_value_cols:
        raise ValueError(f"None of {VALUE_COLUMNS} found in input columns: {list(df.columns)}")

    # Step 1: one row per (PartitionId, 5-min Timestamp) -- max across replicas
    agg1 = {c: 'max' for c in present_value_cols}
    df = df.groupby(['PartitionId', 'Timestamp']).agg(agg1).reset_index()
    print(f"After dedup group-by: {df.shape}")

    # Step 2: assign 2-hour blocks and aggregate again
    df['BlockTimestamp'] = df['Timestamp'].apply(timestamp_to_2hour_block)
    agg2 = {c: 'max' for c in present_value_cols}
    agg2['Timestamp'] = 'first'
    df = df.groupby(['PartitionId', 'BlockTimestamp']).agg(agg2).reset_index()
    print(f"After 2-hour block aggregation: {df.shape}")

    return df


# ---------------------------------------------------------------------------
# Feature engineering (per dimension)
# ---------------------------------------------------------------------------

def _process_partition(partition_data, rolling_windows, target_col):
    """Rolling statistics for a single partition on *target_col*."""
    partition_data = partition_data.sort_values('BlockTimestamp')
    values = partition_data[target_col].to_numpy(dtype=np.float64)
    n = len(values)

    for w in rolling_windows:
        if n <= w:
            partition_data[f'rolling_mean_{w}'] = np.mean(values)
            continue
        rm = np.full(n, np.nan)
        for i in range(w - 1):
            rm[i] = np.mean(values[:i + 1])
        for i in range(w - 1, n):
            rm[i] = np.mean(values[i - w + 1:i + 1])
        partition_data[f'rolling_mean_{w}'] = rm

    sw = min(12, n)
    if sw < 3:
        partition_data['skewness'] = 0.0
        partition_data['kurtosis'] = 0.0
    else:
        sk, ku = np.zeros(n), np.zeros(n)
        for i in range(n):
            lo = max(0, i - sw + 1) if i >= sw - 1 else 0
            wd = values[lo:i + 1]
            if len(wd) < 3:
                continue
            m, s = np.mean(wd), np.std(wd)
            if s > 0:
                z = (wd - m) / s
                sk[i] = np.mean(z ** 3)
                ku[i] = np.mean(z ** 4) - 3
        partition_data['skewness'] = sk
        partition_data['kurtosis'] = ku

    return partition_data


def create_features(df, target_col):
    """Time-based and rolling features for *target_col*."""
    print(f"  Creating features for {target_col} ...")
    df['hour_of_day'] = df['BlockTimestamp'].dt.hour
    df['minute_of_hour'] = df['BlockTimestamp'].dt.minute
    df['day_of_week'] = df['BlockTimestamp'].dt.weekday
    df['is_weekend'] = (df['day_of_week'] >= 5).astype(int)

    df = df.sort_values(['PartitionId', 'BlockTimestamp'])
    rolling_windows = [2, 4, 8, 12]

    results = []
    for _, grp in df.groupby('PartitionId'):
        results.append(_process_partition(grp.copy(), rolling_windows, target_col))
    df = pd.concat(results, ignore_index=True)

    fill_cols = [f'rolling_mean_{w}' for w in rolling_windows] + ['skewness', 'kurtosis']
    df[fill_cols] = df.groupby('PartitionId')[fill_cols].transform(
        lambda x: x.ffill().fillna(0)
    )
    df = df.fillna(0)
    return df


def create_next_day_target(df, target_col):
    """Target = same (PartitionId, BlockTimestamp) shifted forward by 1 day.

    Uses a merge instead of index lookup to avoid tz-aware key matching issues.
    """
    print(f"  Creating next-day target for {target_col} ...")
    df = df.sort_values(['PartitionId', 'BlockTimestamp'])

    future = df[['PartitionId', 'BlockTimestamp', target_col]].copy()
    future['BlockTimestamp'] = future['BlockTimestamp'] - pd.Timedelta(days=1)
    future = future.rename(columns={target_col: 'NextDay_Load'})

    if 'NextDay_Load' in df.columns:
        df = df.drop(columns=['NextDay_Load'])
    df = df.merge(future, on=['PartitionId', 'BlockTimestamp'], how='left')

    missing = df['NextDay_Load'].isna().sum()
    n_dates = df['BlockTimestamp'].dt.date.nunique()
    print(f"  {missing} missing values in NextDay_Load ({missing / len(df) * 100:.1f}%)")
    if missing == len(df):
        print(f"  WARNING: data spans only {n_dates} unique date(s) -- need >= 2 days for next-day targets")
    return df


# ---------------------------------------------------------------------------
# Model training
# ---------------------------------------------------------------------------

def tune_model(X_train, y_train, quantile, output_dir='.', n_iter=10, cv=2):
    os.makedirs(output_dir, exist_ok=True)
    param_dist = {
        'num_leaves': randint(11, 50),
        'learning_rate': uniform(0.01, 0.05),
        'n_estimators': randint(50, 200),
        'max_depth': randint(3, 10),
        'min_child_samples': randint(5, 20),
        'subsample': uniform(0.6, 0.4),
        'colsample_bytree': uniform(0.6, 0.4),
        'reg_alpha': uniform(0.0, 0.2),
        'reg_lambda': uniform(0.0, 0.2),
    }
    model = lgb.LGBMRegressor(
        objective='quantile', alpha=quantile, n_jobs=1, verbose=-1,
    )
    scorer = make_scorer(pinball_loss, greater_is_better=False, quantile=quantile)
    search = RandomizedSearchCV(
        model, param_distributions=param_dist, n_iter=n_iter, cv=cv,
        scoring=scorer, verbose=1, random_state=42,
        return_train_score=False, n_jobs=-1,
    )
    try:
        search.fit(X_train, y_train)
    except Exception as e:
        print(f"  Error fitting quantile {quantile}: {e}")
        gc.collect()
        raise

    log_path = os.path.join(output_dir, f"log_{int(quantile * 100)}.txt")
    with open(log_path, 'w') as f:
        f.write("Hyperparameters and corresponding loss:\n")
        for trial, score in zip(search.cv_results_['params'],
                                search.cv_results_['mean_test_score']):
            f.write(f"{trial} -> Loss: {-score}\n")

    print(f"  Best params q={quantile}: {search.best_params_}  CV={search.best_score_:.6f}")
    best = search.best_estimator_
    del search
    gc.collect()
    return best


def train_models(df, target_col, feature_cols, output_dir='.',
                 quantiles=None, n_iter=10, cv=2):
    if quantiles is None:
        quantiles = [0.5, 0.9, 0.995]
    os.makedirs(output_dir, exist_ok=True)
    df = df.dropna(subset=feature_cols + [target_col])
    X, y = df[feature_cols], df[target_col]
    print(f"  Training data: X={X.shape}, y range [{y.min():.2f}, {y.max():.2f}]")

    if y.nunique() <= 1:
        print(f"  WARNING: target has {y.nunique()} unique value(s) -- skipping model training")
        return {}

    models = {}
    for q in quantiles:
        print(f"  Training quantile {q} ...")
        try:
            m = tune_model(X, y, q, output_dir=output_dir, n_iter=n_iter, cv=cv)
            if m is None:
                continue
            joblib.dump(m, os.path.join(output_dir, f"model_q{int(q * 100)}.pkl"))

            try:
                plt.figure(figsize=(10, 6))
                lgb.plot_importance(m, max_num_features=15)
                plt.title(f"Feature Importance (q={q})")
                plt.tight_layout()
                plt.savefig(os.path.join(output_dir, f"feature_importance_q{int(q * 100)}.png"))
            except Exception:
                pass
            finally:
                plt.close()

            models[q] = m
        except Exception as e:
            print(f"  ERROR training q={q}: {e}")

    print(f"  Trained {len(models)} models: {list(models.keys())}")
    return models


# ---------------------------------------------------------------------------
# Prediction
# ---------------------------------------------------------------------------

def predict_multiple_days(models, test_df, feature_cols,
                          target_col='NextDay_Load', num_days=6,
                          output_dir='.'):
    print(f"  Generating predictions for {num_days} days ...")
    working_df = test_df.copy()
    start_date = working_df['BlockTimestamp'].min().date()
    daily_preds = []

    for d in range(num_days):
        cur = start_date + timedelta(days=d)
        nxt = cur + timedelta(days=1)
        mask = working_df['BlockTimestamp'].dt.date == cur
        day_data = working_df[mask]
        if day_data.empty:
            print(f"  No data for {cur}, skipping")
            continue

        X = day_data[feature_cols]
        out = pd.DataFrame({
            'TIMESTAMP': day_data['BlockTimestamp'].values,
            'PartitionId': day_data['PartitionId'].values,
            'Actual_load': day_data[target_col].values if target_col in day_data.columns else np.nan,
            'forecast_date': nxt,
            'prediction_day_index': d + 1,
        })
        for q, m in models.items():
            try:
                out[f'pred_load_{int(q * 100)}'] = m.predict(X)
            except Exception as e:
                print(f"  Prediction error q={q}: {e}")
                out[f'pred_load_{int(q * 100)}'] = np.nan
        daily_preds.append(out)

    if not daily_preds:
        print("  WARNING: no predictions generated")
        return None

    all_preds = pd.concat(daily_preds, ignore_index=True)
    out_path = os.path.join(output_dir, 'predictions_by_timestamp_partition.csv')
    all_preds.to_csv(out_path, index=False)
    print(f"  Predictions saved to {out_path}  shape={all_preds.shape}")
    return all_preds


# ---------------------------------------------------------------------------
# Evaluation
# ---------------------------------------------------------------------------

def evaluate_models(models, X_test, y_test, output_dir='.'):
    results = {}
    plt.figure(figsize=(12, 8))
    for quantile, model in models.items():
        y_pred = model.predict(X_test)
        loss = pinball_loss(y_test, y_pred, quantile)
        results[quantile] = loss
        plt.scatter(y_test, y_pred, alpha=0.5,
                    label=f'Q{int(quantile * 100)} (Loss: {loss:.4f})')

    plt.plot([min(y_test), max(y_test)], [min(y_test), max(y_test)], 'k--')
    plt.xlabel('Actual Values')
    plt.ylabel('Predicted Values')
    plt.title('Actual vs Predicted for Different Quantiles')
    plt.legend()
    plt.tight_layout()
    plt.savefig(os.path.join(output_dir, 'model_evaluation.png'))
    plt.close()

    with open(os.path.join(output_dir, 'evaluation_results.txt'), 'w') as f:
        f.write("Evaluation Results (Pinball Loss):\n")
        for quantile, loss in results.items():
            f.write(f"Quantile {quantile}: {loss}\n")
    return results


# ---------------------------------------------------------------------------
# Per-dimension full pipeline
# ---------------------------------------------------------------------------

def run_single_dimension(preprocessed_csv, target_col, output_dir,
                         quantiles, n_iter, cv, forecast_days):
    """Complete train-predict pipeline for one target column."""
    dim_dir = os.path.join(output_dir, target_col)
    os.makedirs(dim_dir, exist_ok=True)
    t0 = time.time()

    print(f"\n{'=' * 60}")
    print(f"DIMENSION: {target_col}")
    print(f"{'=' * 60}")

    df = pd.read_csv(preprocessed_csv)
    df['BlockTimestamp'] = pd.to_datetime(df['BlockTimestamp'], utc=True)
    df['Timestamp'] = pd.to_datetime(df['Timestamp'], utc=True)

    df = create_features(df, target_col)
    log_memory_usage(f"  [{target_col}] After features")

    df = create_next_day_target(df, target_col)

    for c in df.columns:
        if c not in ('PartitionId', 'BlockTimestamp', 'Timestamp'):
            df[c] = pd.to_numeric(df[c], errors='coerce').fillna(0)

    # Dynamic train / test split
    dates = sorted(df['BlockTimestamp'].dt.date.unique())
    if len(dates) <= forecast_days:
        split_idx = max(1, len(dates) - 1)
    else:
        split_idx = len(dates) - forecast_days
    cutoff = pd.Timestamp(dates[split_idx], tz=df['BlockTimestamp'].dt.tz)
    print(f"  Train < {cutoff.date()}, Test >= {cutoff.date()}")

    train_df = df[df['BlockTimestamp'] < cutoff].copy()
    test_df = df[df['BlockTimestamp'] >= cutoff].copy()
    del df
    gc.collect()

    train_df = train_df.dropna(subset=['NextDay_Load'])
    print(f"  Train: {len(train_df)}, Test: {len(test_df)}")

    exclude = {'PartitionId', 'BlockTimestamp', 'Timestamp', 'NextDay_Load'} | set(VALUE_COLUMNS)
    feature_cols = [c for c in train_df.columns if c not in exclude]
    print(f"  Features ({len(feature_cols)}): {feature_cols}")

    models = train_models(
        train_df, 'NextDay_Load', feature_cols,
        output_dir=dim_dir, quantiles=quantiles, n_iter=n_iter, cv=cv,
    )
    del train_df
    gc.collect()

    if not models:
        print(f"  ERROR: no models for {target_col}")
        return None

    preds = predict_multiple_days(
        models, test_df, feature_cols,
        target_col='NextDay_Load', num_days=forecast_days, output_dir=dim_dir,
    )
    print(f"  {target_col} done in {time.time() - t0:.1f}s")
    return preds


# ---------------------------------------------------------------------------
# Orchestrator -- preprocess once, run 4 dimensions in parallel
# ---------------------------------------------------------------------------

def run_all_dimensions(input_file, output_dir='./outputs',
                       quantiles=None, n_iter=10, cv=2,
                       forecast_days=3):
    if quantiles is None:
        quantiles = [0.001, 0.25, 0.5, 0.75, 0.9, 0.995]

    t0 = time.time()
    log_memory_usage("Initial memory usage")

    df = preprocess_data(input_file)
    os.makedirs(output_dir, exist_ok=True)
    preprocessed_csv = os.path.join(output_dir, '_preprocessed.csv')
    df.to_csv(preprocessed_csv, index=False)
    print(f"Preprocessed data saved to {preprocessed_csv}")
    del df
    gc.collect()

    dimensions = [c for c in VALUE_COLUMNS]

    with ProcessPoolExecutor(max_workers=len(dimensions)) as pool:
        futures = {
            pool.submit(
                run_single_dimension,
                preprocessed_csv, dim, output_dir,
                quantiles, n_iter, cv, forecast_days,
            ): dim
            for dim in dimensions
        }
        results = {}
        for fut in futures:
            dim = futures[fut]
            try:
                results[dim] = fut.result()
                print(f"[OK] {dim} completed")
            except Exception as e:
                print(f"[FAIL] {dim}: {e}")

    if os.path.exists(preprocessed_csv):
        os.remove(preprocessed_csv)

    print(f"\nAll dimensions completed in {time.time() - t0:.1f}s")
    return results


# ---------------------------------------------------------------------------
# CLI entry point
# ---------------------------------------------------------------------------

if __name__ == '__main__':
    parser = argparse.ArgumentParser(
        description='Multi-dimension quantile forecasting on testbed data',
    )
    parser.add_argument('--input', type=str, required=True,
                        help='Input xlsx or csv file (e.g. testbed_10march_16march_part1.xlsx)')
    parser.add_argument('--output_dir', type=str, default='./outputs',
                        help='Directory for all outputs (models, plots, predictions)')
    parser.add_argument('--quantiles', type=float, nargs='+',
                        default=[0.001, 0.25, 0.5, 0.75, 0.9, 0.995],
                        help='Quantiles to predict')
    parser.add_argument('--n_iter', type=int, default=10,
                        help='Iterations for hyperparameter search')
    parser.add_argument('--cv', type=int, default=2,
                        help='Cross-validation folds')
    parser.add_argument('--forecast_days', type=int, default=3,
                        help='Number of days to forecast')

    args = parser.parse_args()

    results = run_all_dimensions(
        args.input,
        output_dir=args.output_dir,
        quantiles=args.quantiles,
        n_iter=args.n_iter,
        cv=args.cv,
        forecast_days=args.forecast_days,
    )
    for dim, pred in results.items():
        status = f"shape={pred.shape}" if pred is not None else "FAILED"
        print(f"  {dim}: {status}")
    print("Done.")
