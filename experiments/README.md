# LoadStar + Luna-Orbit Experiments

These experiment configurations mirror the experiments section (6.1 - 6.6) of the article. You can run **all of them in one touch** from the repo root: `./run_pipeline.sh --experiments`.

* **Base config** (same across sets, unless varied)
  * `skewness=1, load=6M, nodes=196, CPU=6M, memory=389938, storage=540000, soft=N, policy=9, forecast_window=12, migration=1 (Binary)`
  * See `loadstar/Policy Executor/INPUT_FORMAT.md` for parameter details

* **Migration policy** (last value in input)
  * `1 = Binary (Orbit default, from paper), 2 = Lightest, 3 = Heaviest`
  * Change the last line in any input config file to run 1/2/3

* Run the policy executor from repo root, e.g.:
```bash
cd "loadstar/Policy Executor"
./policy_executor ../../luna/testbed.csv 10 \
  ../../outputs/MaxCounterCPU/forecast_0.9.csv \
  ../../outputs/MaxCounterValueMemory/forecast_0.9.csv \
  ../../outputs/TotalUsageInMB/forecast_0.9.csv \
  ../../outputs/sum_TotalRequestCharge/forecast_0.9.csv \
  < ../../experiments/set2_alpha/input_alpha4.txt > output.txt
```

---

## Sec 6.6: Tuning forecast percentile (Set 1)

**Paper:** P10, P25, P50, P75, P90 evaluated; P50 chosen as default.

**What varies:** Forecast file path (percentile), not the config file. Use **same** input: `set1_percentile/input_base.txt`. Pass the chosen forecast CSVs. Here **dimension** is one of the four **resource dimensions** (subfolders under `outputs/`): `sum_TotalRequestCharge`, `MaxCounterCPU`, `MaxCounterValueMemory`, `TotalUsageInMB


| Paper percentile | Forecast files (per dimension) |
|------------------|--------------------------------|
| P90              | `outputs/<dimension>/forecast_0.9.csv` |
| P75              | `outputs/<dimension>/forecast_0.75.csv` |
| P50 (default)    | `outputs/<dimension>/forecast_0.5.csv` |
| P25              | `outputs/<dimension>/forecast_0.25.csv` |
| P01 (conservative) | `outputs/<dimension>/forecast_0.01.csv` |

Luna must be run with the desired quantiles (e.g. `--quantiles 0.01 0.25 0.5 0.75 0.9`). One-touch uses `forecast_0.9.csv` only for set1.

---

## Sec 6.4: Tuning skewness parameter α (Set 2)

**Paper:** “Weightage given to the skewness in Algorithm 3.” α from 0.1 to 10.0; default α = 4.

**What varies:** Skewness (first value in config). Rest = base.

| Paper | File |
|-------|------|
| α = 1 | `set2_alpha/input_alpha1.txt` |
| α = 2 | `set2_alpha/input_alpha2.txt` |
| α = 4 (default) | `set2_alpha/input_alpha4.txt` |
| α = 8 | `set2_alpha/input_alpha8.txt` |

---

## Sec 6.3: Resource efficiency gains / node footprint (Set 3)

**Paper:** “Reduce the number of nodes in steps of 5%, from 100% to 65%.”

**What varies:** Number of nodes (third value in config).

| Paper | File | Nodes |
|-------|------|-------|
| 100%  | `set3_nodes/input_nodes196.txt` | 196 |
| ~90%  | `set3_nodes/input_nodes176.txt` | 176 |
| ~80%  | `set3_nodes/input_nodes157.txt` | 157 |
| ~70%  | `set3_nodes/input_nodes137.txt` | 137 |
| ~65%  | `set3_nodes/input_nodes127.txt` | 127 |

---

## Sec 6.5: Tuning forecast window (Set 4)

**Paper:** “Time windows of 0.5h to 24h”; default 2h.

**What varies:** Forecast window size (11th value in config).

| Paper | File | Steps | Approx. |
|-------|------|-------|---------|
| ~1h   | `set4_forecast_window/input_window12.txt`  | 12 | 1h  |
| 2h (default) | `set4_forecast_window/input_window24.txt` | 24 | 2h  |
| ~3h   | `set4_forecast_window/input_window36.txt`  | 36 | 3h  |
| ~6h   | `set4_forecast_window/input_window72.txt`  | 72 | 6h  |

---

## Running experiments

For One-touch executions, see [REPRODUCIBILITY.md](../REPRODUCIBILITY.md) for more details.

1. **One-touch (all paper experiments):**  
   From repo root: `./run_pipeline.sh --experiments` — runs full pipeline then all four sets (Sec 6.6, 6.4, 6.3, 6.5). Outputs in `experiments/outputs/`.

2. **One-touch, specific sections:**  
   `./run_pipeline.sh --set set2_alpha set4_forecast_window` (or `set1_percentile`, `set3_nodes`). Use `--no-pipeline` if testbed and forecasts already exist.

3. **One-off (single config):**  
   Copy the chosen experiment input to `loadstar/Policy Executor/input.txt`, then run the pipeline or the policy executor command as in [REPRODUCIBILITY.md](../REPRODUCIBILITY.md).

4. **Batch via script:**  
   `./run_experiments.sh` runs Set2, Set3, Set4; `./run_experiments.sh set2_alpha set4_forecast_window` runs only those sets. Outputs go to `experiments/outputs/`.

5. **Varying migration policy:**  
   Edit the last line of the experiment input file to 1, 2, or 3 (Binary, Lightest, Heaviest) and re-run.
