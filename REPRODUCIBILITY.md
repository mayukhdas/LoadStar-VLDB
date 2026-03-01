# Reproducibility Guide for Luna-Orbit & LoadStar

This *quick start* document describes how to functionally reproduce the article from a clean slate. It provides a **one-touch run** script, and optional **step-by-step** execution details and describes the **artifact locations**.

A more detailed workflow in provided in [INSTRUCTIONS.md](INSTRUCTIONS.md).

---

## Prerequisites
A standalone Linux machine or VM with:

- **Python 3.8+**
- **g++** (C++17)
- **Git**
- **Internet Connectivity** to download trace files from Zenodo
- **Optional:** `sudo` for `apt-get install libboost-dev` during setup

---

## One-touch run

From the **repository root**:

```bash
chmod +x run_pipeline.sh
./run_pipeline.sh
```

The script will:

1. **Setup & build** (~5 min): Run `build/setup.sh`, which sets up `venv` virtual environment, installs Python dependencies and `libboost-dev`, creates output directories) and `build/make.sh` (compile Policy Executor).
2. **Download & combine testbed** (~20 min): Run `download_and_combine_testbed.py`, which extracts the CosmosDB Excel trace data files (parts 1-29) from Zenodo, imports them into a local `testbed.csv` after deduplication, and copies it to `luna/testbed.csv`.
3. **Luna forecasting** (~35 min): Run `luna/forecasting_model.py` on `luna/testbed.csv` for all four dimensions (load, CPU, memory, storage) to perform multi-dimension quantile forecasting, and writes outputs into `outputs/`.
4. **Expand forecasts** (~20 min): Run `luna/expand_and_save_forecasts.py` to expand 2-hour block predictions to 5-minute intervals; one file per quantile is written, e.g. `outputs/<dimension>/forecast_0.01.csv`, `forecast_0.25.csv`, `forecast_0.5.csv`, `forecast_0.75.csv`, `forecast_0.9.csv`.
5. **LoadStar Policy Simulator** (~1.5–2.5 h per run): Run the LoadStar C++ policy executor with `loadstar/Policy Executor/input.txt` as input (or `input.txt.sample` if missing) and write `output.txt`, `MigrationInfo.csv`, and `*_NodeState.csv`.
6. **DRV profile generation** (~15 min): Copy the latest `*_NodeState.csv` into `loadstar/Metric Processor/NodeState.csv` and, if `loadstar/Metric Processor/error_rates.csv` exists, run `drv_profiles_comparisons.py` to produce DRV profile plots and metrics; otherwise the step is skipped (see INSTRUCTIONS.md and Zenodo for error-rate data).

Progress from the LoadStar policy executor is printed to `stderr` (visible in the terminal). `stdout` is redirected to `output.txt`.

---

## Approximate Running Times

The following timings were measured on **AWS us-east-1**, on a `t3.2xlarge` VM (8 vCPUs, 32 GiB RAM). Your runtimes may vary with hardware and network.

| Step | Approximate time |
|------|------------------|
| Setup and build | ~5 min |
| Download & combine testbed | ~20 min |
| Luna forecasting | ~35 min |
| Expand forecasts | ~20 min |
| LoadStar Policy Simulator | ~1.5–2.5 h **per experiment** |

**Full pipeline execution (without specific experiments):** ~1.5–2 h.  
**Pipeline + all experiments** (four experiment sets, many configs): add ~1.5–2.5 h per experiment run; use `--no-pipeline --experiments` after the first run to re-run only experiments. 
For more details, see [experiments/README.md](experiments/README.md).

---

## Expected artifacts after a successful run

| Artifact | Location |
|----------|----------|
| Testbed CSV | `luna/testbed.csv` |
| Forecasts (per dimension, per quantile) | `outputs/<dimension>/forecast_0.01.csv`, `forecast_0.25.csv`, `forecast_0.5.csv`, `forecast_0.75.csv`, `forecast_0.9.csv` (pipeline generates P01, P25, P50, P75, P90) |
| Policy stdout | `loadstar/Policy Executor/output.txt` |
| Migration log | `loadstar/Policy Executor/MigrationInfo.csv` |
| Node state log | `loadstar/Policy Executor/<config_prefix>_NodeState.csv` (or under `luna/` depending on config) |
| DRV profiles | `loadstar/Metric Processor/` (plots and metrics; requires `error_rates.csv` there) |

---

## Configuration of `input.txt` for LoadStar

The LoadStar policy simulator uses different policy configuration parameters from  `input.txt` (e.g. WorstFitWithCPUMemoryRUPrediction, skewness, capacities, node count, policy 9, forecast window, migration policy). 
See [INPUT_FORMAT.md](loadstar/Policy%20Executor/INPUT_FORMAT.md)) for full details.
If `input.txt` is missing, the one-touch pipeline copies `input.txt.sample` to `input.txt`.

---

## Experiments

These steps reproduce the experiments present in the VLDB article by configuring the right set of parameters for the policy simulator, forecast and testbed.
* **Sec 6.3** Resource efficiency gains (node footprint reduction)
*  **Sec 6.4** Tuning skewness parameter α
*  **Sec 6.5** Tuning forecast window
*  **Sec 6.6** Tuning forecast percentile

The **base configuration** is in `loadstar/Policy Executor/input.txt.sample`: skewness=1, load=6M, nodes=196, CPU=6M, memory=389938, storage=540000, soft=N, policy=9, forecast_window=12, migration=1 (Binary). You can vary **migration policy** in any input by changing the last line: **1** = Binary (paper’s Orbit default), **2** = Lightest, **3** = Heaviest. 

For more details, see [experiments/README.md](experiments/README.md).

All commands from the **repository root**.

* **To run all experiments in one touch:**  
`./run_pipeline.sh --experiments`  
runs the full pipeline (setup -> testbed -> Luna -> expand -> one policy run) and then every experiment in all four sets below. Outputs go to `experiments/outputs/`
* To run only specific paper sections, use `./run_pipeline.sh --set set3_nodes set4_forecast_window` (see table at the end).
* **Pipeline options:**
  * `--experiments` runs all four sets 
  * `--set SET [SET ...]` runs only the listed sets (e.g. `set1_percentile`, `set2_alpha`, `set3_nodes`, `set4_forecast_window`)
  * `--no-pipeline` skips pipeline steps 1–5
  * `--help` for a short summary

**Outputs:** Batch runs write stdout to `experiments/outputs/<set>_<input_name>.txt`. Migration and node-state CSVs are in `loadstar/Policy Executor/`.

| Goal | Command |
|------|--------|
| **One-touch: pipeline + all paper experiments (Sec 6.3–6.6)** | `./run_pipeline.sh --experiments` |
| **Pipeline + only some sections** | `./run_pipeline.sh --set set2_alpha set4_forecast_window` |
| **Experiments only** (testbed & forecasts already exist) | `./run_pipeline.sh --no-pipeline --experiments` |
| **Experiments only, specific sections** | `./run_pipeline.sh --no-pipeline --set set3_nodes` |
| **Standalone: all sets** | `./run_experiments.sh set1_percentile set2_alpha set3_nodes set4_forecast_window` |
| **Standalone: default sets** (6.4, 6.3, 6.5) | `./run_experiments.sh` |
| **Standalone: one or more sets** | `./run_experiments.sh set2_alpha set4_forecast_window` |
| **Single experiment (manual)** | `cd "loadstar/Policy Executor"` then `./policy_executor ../../luna/testbed.csv 10 ../../outputs/MaxCounterCPU/forecast_0.9.csv ../../outputs/MaxCounterValueMemory/forecast_0.9.csv ../../outputs/TotalUsageInMB/forecast_0.9.csv ../../outputs/sum_TotalRequestCharge/forecast_0.9.csv < ../../experiments/set2_alpha/input_alpha4.txt > output.txt` |

---

## Description of Individual Pipeline Steps

### Step 1: Setup and build

- **Scripts:** `build/setup.sh`, `build/make.sh`
- **setup.sh:** Creates `build/venv`, installs `build/requirements.txt`, installs system `libboost-dev`, creates `outputs` dirs.
- **make.sh:** Compiles `loadstar/Policy Executor/main.cpp` → `loadstar/Policy Executor/policy_executor`.

### Step 2: Download and combine testbed

- **Script:** `download_and_combine_testbed.py` (repo root)
- **Input:** Zenodo (testbed Excel parts 1–29).
- **Output:** `testbed.csv` at repo root; pipeline copies it to `luna/testbed.csv`.
- **Columns:** `Timestamp, PartitionId, UniqueId, ReplicaId, sum_TotalRequestCharge, MaxCounterCPU, MaxCounterValueMemory, TotalUsageInMB`

### Step 3: Luna forecasting

- **Script:** `luna/forecasting_model.py`
- **Command (from pipeline):**  
  `python luna/forecasting_model.py --input luna/testbed.csv --output_dir outputs --forecast_days 3 --n_iter 10 --cv 2`
- **Output:** Under `outputs/`: one subdir per dimension (`sum_TotalRequestCharge`, `MaxCounterCPU`, `MaxCounterValueMemory`, `TotalUsageInMB`) with models, predictions, and `predictions_by_timestamp_partition.csv`.

### Step 4: Expand and save forecasts

- **Script:** `luna/expand_and_save_forecasts.py`
- **Input:** `outputs/*/predictions_by_timestamp_partition.csv` (one CSV per dimension; columns include `pred_load_1`, `pred_load_25`, `pred_load_50`, `pred_load_75`, `pred_load_90`).
- **Output:** One expanded file per quantile per dimension, e.g. `outputs/<dimension>/forecast_0.01.csv`, `forecast_0.25.csv`, `forecast_0.5.csv`, `forecast_0.75.csv`, `forecast_0.9.csv` (used by the policy executor and Sec 6.6 experiments).

### Step 5: LoadStar Policy Simulator

- **Binary:** `loadstar/Policy Executor/policy_executor`
- **Command (from pipeline):**  
  `./policy_executor ../../luna/testbed.csv 10 <four forecast CSV paths> < input.txt > output.txt`
- **Input:** Stdin from `loadstar/Policy Executor/input.txt` (one value per line; see [INPUT_FORMAT.md](loadstar/Policy%20Executor/INPUT_FORMAT.md) or sample).
- **Output:**  
  - `loadstar/Policy Executor/output.txt`  
  - `loadstar/Policy Executor/MigrationInfo.csv`  
  - `loadstar/Policy Executor/<prefix>_NodeState.csv`


---

## Manual Step-by-step Execution

If you prefer to run stages yourself or debug a step:

1. **Setup (once)**  
   `cd build && ./setup.sh && cd ..`  
   Activate venv: `source build/venv/bin/activate`

2. **Build (once, or after C++ changes)**  
   `cd build && ./make.sh && cd ..`

3. **Testbed**  
   `python download_and_combine_testbed.py`  
   `cp testbed.csv luna/testbed.csv`

4. **Forecasting**  
   `python luna/forecasting_model.py --input luna/testbed.csv --output_dir outputs --forecast_days 3`

5. **Expand**  
   `python luna/expand_and_save_forecasts.py`

6. **Policy executor**  
   `cd "loadstar/Policy Executor"`  
   `./policy_executor ../../luna/testbed.csv 10 ../../outputs/MaxCounterCPU/forecast_0.9.csv ../../outputs/MaxCounterValueMemory/forecast_0.9.csv ../../outputs/TotalUsageInMB/forecast_0.9.csv ../../outputs/sum_TotalRequestCharge/forecast_0.9.csv < input.txt > output.txt`

---

## Troubleshooting

- **Setup fails (e.g. libboost-dev):** Install manually: `sudo apt-get install libboost-dev`, then re-run setup or the pipeline.
- **Download fails:** Check network and Zenodo availability; `download_and_combine_testbed.py` uses Zenodo records.
- **Forecasting OOM:** Reduce `--forecast_days` or run on a machine with more RAM.
- **Policy executor assert/crash:** Ensure `luna/testbed.csv` and all four `outputs/<dim>/forecast_0.9.csv` exist and that node thresholds in `input.txt` are large enough (see paper/INPUT_FORMAT for suggested values).
- **No progress in terminal:** Progress is on **stderr**; with `2>&1` it goes to `output.txt`. Run without redirecting stderr to see progress in the terminal.

---

## Additional Details
- [experiments/README.md](experiments/README.md): Description of various experiment configurations from the paper.
- [loadstar/Policy Executor/INPUT_FORMAT.md](loadstar/Policy%20Executor/INPUT_FORMAT.md): Details of parameters and options for LoadStart Policy Simulator.
- [CosmosDB Trace Dataset from Zenodo](https://zenodo.org/records/16539984) CosmosDB Replica traces and error rates from testbeds used in the article.
- [INSTRUCTIONS.md](INSTRUCTIONS.md): Description of full setup, data flow, and analysis phases.
