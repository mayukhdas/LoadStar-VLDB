#!/bin/bash
#
# One-touch reproducibility pipeline for LoadStar & Luna-Orbit.
# Run from the repository root: ./run_pipeline.sh [options]
#
# Steps: 1) Setup env & build  2) Download & combine testbed  3) Luna forecasting
#        4) Expand forecasts    5) Policy executor  6) DRV profile generation  [7) Optional: run experiments]
#
# Options:
#   --experiments          After the pipeline, run all experiment sets (set1_percentile, set2_alpha, set3_nodes, set4_forecast_window).
#   --set SET [SET ...]    After the pipeline, run only these sets (e.g. --set set2_alpha set3_nodes).
#   --no-pipeline          Skip pipeline steps 1-5; only run experiments (requires --experiments or --set). Use when testbed and forecasts already exist.
#   --help                 Show this help and exit.
#

set -e

REPO_ROOT="$(cd "$(dirname "$0")" && pwd)"
cd "$REPO_ROOT"

RUN_EXPERIMENTS=false
RUN_SETS=()
SKIP_PIPELINE=false

while [[ $# -gt 0 ]]; do
    case "$1" in
        --experiments|--all-experiments)
            RUN_EXPERIMENTS=true
            RUN_SETS=(set1_percentile set2_alpha set3_nodes set4_forecast_window)
            shift
            ;;
        --set)
            shift
            RUN_EXPERIMENTS=true
            RUN_SETS=()
            while [[ $# -gt 0 && ! "$1" =~ ^-- ]]; do
                RUN_SETS+=("$1")
                shift
            done
            ;;
        --no-pipeline)
            SKIP_PIPELINE=true
            shift
            ;;
        --help|-h)
            echo "Usage: $0 [options]"
            echo ""
            echo "  --experiments          After pipeline, run all experiment sets (set1, set2, set3, set4)."
            echo "  --set SET [SET ...]    After pipeline, run only these sets (e.g. --set set2_alpha set3_nodes)."
            echo "  --no-pipeline          Skip steps 1-5; only run experiments (use with --experiments or --set)."
            echo "  --help                 Show this help."
            exit 0
            ;;
        *)
            echo "Unknown option: $1" >&2
            echo "Use --help for usage."
            exit 1
            ;;
    esac
done

echo "=============================================="
echo "LoadStar + Luna-Orbit: One-touch pipeline"
echo "=============================================="

if [ "$SKIP_PIPELINE" = true ]; then
    if [ "$RUN_EXPERIMENTS" = false ]; then
        echo "Error: --no-pipeline requires --experiments or --set."
        exit 1
    fi
    echo "Skipping pipeline (--no-pipeline). Running experiments only."
else
# ---------------------------------------------------------------------------
# Step 1: Setup and build
# ---------------------------------------------------------------------------
echo ""
echo "[1/5] Setup and build..."
if [ ! -d "build" ]; then
    echo "Error: build/ directory not found. Run from repository root."
    exit 1
fi
cd build
if [ ! -d "venv" ]; then
    bash setup.sh
else
    echo "Using existing venv in build/"
    source venv/bin/activate
fi
bash make.sh
cd "$REPO_ROOT"

PYTHON="${REPO_ROOT}/build/venv/bin/python"
if [ ! -x "$PYTHON" ]; then
    echo "Error: venv Python not found at $PYTHON"
    exit 1
fi

# ---------------------------------------------------------------------------
# Step 2: Download and combine testbed
# ---------------------------------------------------------------------------
echo ""
echo "[2/5] Download and combine testbed..."
$PYTHON download_and_combine_testbed.py
if [ ! -f "testbed.csv" ]; then
    echo "Error: testbed.csv was not created."
    exit 1
fi
mkdir -p luna
cp testbed.csv luna/testbed.csv
echo "  -> luna/testbed.csv"

# ---------------------------------------------------------------------------
# Step 3: Luna forecasting (all dimensions)
# ---------------------------------------------------------------------------
echo ""
echo "[3/5] Luna forecasting (all dimensions, quantiles 0.01 0.25 0.5 0.75 0.9)..."
mkdir -p outputs
$PYTHON luna/forecasting_model.py \
    --input luna/testbed.csv \
    --output_dir outputs \
    --quantiles 0.01 0.25 0.5 0.75 0.9 \
    --forecast_days 3 \
    --n_iter 10 \
    --cv 2

# ---------------------------------------------------------------------------
# Step 4: Expand and save forecasts
# ---------------------------------------------------------------------------
echo ""
echo "[4/5] Expand and save forecasts..."
$PYTHON luna/expand_and_save_forecasts.py

# ---------------------------------------------------------------------------
# Step 5: Policy executor
# ---------------------------------------------------------------------------
echo ""
echo "[5/5] Policy executor..."
EXEC_DIR="${REPO_ROOT}/loadstar/Policy Executor"
INPUT_TXT="${EXEC_DIR}/input.txt"
if [ ! -f "$INPUT_TXT" ]; then
    if [ -f "${EXEC_DIR}/input.txt.sample" ]; then
        cp "${EXEC_DIR}/input.txt.sample" "$INPUT_TXT"
        echo "  Using input.txt.sample as input.txt"
    else
        echo "Error: $INPUT_TXT not found and no input.txt.sample available."
        exit 1
    fi
fi
cd "$EXEC_DIR"
./policy_executor ../../luna/testbed.csv 10 \
    ../../outputs/MaxCounterCPU/forecast_0.9.csv \
    ../../outputs/MaxCounterValueMemory/forecast_0.9.csv \
    ../../outputs/TotalUsageInMB/forecast_0.9.csv \
    ../../outputs/sum_TotalRequestCharge/forecast_0.9.csv \
    < input.txt 2>&1 | tee output.txt
cd "$REPO_ROOT"

# ---------------------------------------------------------------------------
# Step 6: DRV profile generation
# ---------------------------------------------------------------------------
echo ""
echo "[6/6] DRV profile generation..."
METRIC_PROC="${REPO_ROOT}/loadstar/Metric Processor"
# Executor may write NodeState under EXEC_DIR or under luna/ if prefix contains path
NODESTATE_LATEST=$(ls -t "${EXEC_DIR}"/*_NodeState.csv "${REPO_ROOT}/luna/"*_NodeState.csv 2>/dev/null | head -1)
if [ -n "$NODESTATE_LATEST" ] && [ -f "$NODESTATE_LATEST" ]; then
    cp "$NODESTATE_LATEST" "${METRIC_PROC}/NodeState.csv"
    echo "  Copied $(basename "$NODESTATE_LATEST") to Metric Processor/NodeState.csv"
fi
if [ -f "${METRIC_PROC}/error_rates.csv" ]; then
    cd "$METRIC_PROC"
    $PYTHON drv_profiles_comparisons.py 2>&1 || true
    cd "$REPO_ROOT"
    echo "  DRV profile plots and metrics written to loadstar/Metric Processor/"
else
    echo "  Skipping DRV (error_rates.csv not found in loadstar/Metric Processor). Add it to generate DRV profiles."
fi

echo ""
echo "=============================================="
echo "Pipeline finished."
echo "  - Testbed:        luna/testbed.csv"
echo "  - Forecasts:      outputs/<dimension>/forecast_0.01.csv ... forecast_0.9.csv"
echo "  - Policy output: loadstar/Policy Executor/output.txt"
echo "  - Migrations:     loadstar/Policy Executor/MigrationInfo.csv"
echo "  - DRV profiles:  loadstar/Metric Processor/ (if error_rates.csv present)"
echo "=============================================="
fi

# ---------------------------------------------------------------------------
# Step 7 (optional): Run experiments
# ---------------------------------------------------------------------------
if [ "$RUN_EXPERIMENTS" = true ]; then
    echo ""
    echo "[7/7] Running experiments..."
    if [ ${#RUN_SETS[@]} -eq 0 ]; then
        RUN_SETS=(set1_percentile set2_alpha set3_nodes set4_forecast_window)
    fi
    bash "${REPO_ROOT}/run_experiments.sh" "${RUN_SETS[@]}"
    echo ""
    echo "Experiment outputs: experiments/outputs/"
fi
