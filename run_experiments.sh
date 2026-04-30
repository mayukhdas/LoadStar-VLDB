#!/bin/bash
#
# Batch runner for the paper experiment sets described in experiments/README.md.
# Run from the repository root:
#   ./run_experiments.sh
#   ./run_experiments.sh set1_percentile set4_forecast_window
#
# Default sets (when none are passed): set2_alpha set3_nodes set4_forecast_window
#

set -euo pipefail

REPO_ROOT="$(cd "$(dirname "$0")" && pwd)"
cd "$REPO_ROOT"

EXEC_DIR="${REPO_ROOT}/loadstar/Policy Executor"
POLICY_EXECUTOR="${EXEC_DIR}/policy_executor"
TESTBED="${REPO_ROOT}/luna/testbed.csv"
OUTPUT_DIR="${REPO_ROOT}/experiments/outputs"

DEFAULT_SETS=(set2_alpha set3_nodes set4_forecast_window)
VALID_SETS=(set1_percentile set2_alpha set3_nodes set4_forecast_window)
RUN_SETS=()

latest_nodestate() {
    ls -t "${EXEC_DIR}"/*_NodeState.csv "${REPO_ROOT}/luna/"*_NodeState.csv 2>/dev/null | head -1 || true
}

latest_migration() {
    ls -t "${EXEC_DIR}"/*_MigrationInfo.csv "${REPO_ROOT}/luna/"*_MigrationInfo.csv 2>/dev/null | head -1 || true
}

require_file() {
    local path="$1"
    if [ ! -f "$path" ]; then
        echo "Error: required file not found: $path" >&2
        exit 1
    fi
}

forecast_repo_path() {
    local dimension="$1"
    local quantile="$2"
    printf '%s\n' "${REPO_ROOT}/outputs/${dimension}/forecast_${quantile}.csv"
}

copy_artifact_if_present() {
    local src="$1"
    local dest="$2"
    if [ -n "$src" ] && [ -f "$src" ]; then
        cp "$src" "$dest"
    fi
}

run_one() {
    local label="$1"
    local input_file="$2"
    local quantile="$3"

    local cpu_repo load_repo memory_repo storage_repo
    cpu_repo="$(forecast_repo_path "MaxCounterCPU" "$quantile")"
    load_repo="$(forecast_repo_path "sum_TotalRequestCharge" "$quantile")"
    memory_repo="$(forecast_repo_path "MaxCounterValueMemory" "$quantile")"
    storage_repo="$(forecast_repo_path "TotalUsageInMB" "$quantile")"

    require_file "$input_file"
    require_file "$cpu_repo"
    require_file "$load_repo"
    require_file "$memory_repo"
    require_file "$storage_repo"

    local log_file="${OUTPUT_DIR}/${label}.txt"
    local before_nodestate before_migration after_nodestate after_migration

    before_nodestate="$(latest_nodestate)"
    before_migration="$(latest_migration)"

    echo "  -> ${label}"
    (
        cd "$EXEC_DIR"
        # Keep the invocation order identical to run_pipeline.sh.
        ./policy_executor ../../luna/testbed.csv 10 \
            "../../outputs/MaxCounterCPU/forecast_${quantile}.csv" \
            "../../outputs/MaxCounterValueMemory/forecast_${quantile}.csv" \
            "../../outputs/TotalUsageInMB/forecast_${quantile}.csv" \
            "../../outputs/sum_TotalRequestCharge/forecast_${quantile}.csv" \
            < "$input_file" > "$log_file" 2>&1
    ) || {
        echo "Error: policy_executor failed for ${label}" >&2
        echo "Log: ${log_file}" >&2
        if [ -f "$log_file" ]; then
            echo "--- last 40 log lines ---" >&2
            tail -n 40 "$log_file" >&2 || true
            echo "-------------------------" >&2
        fi
        exit 1
    }

    after_nodestate="$(latest_nodestate)"
    after_migration="$(latest_migration)"

    if [ -n "$after_nodestate" ]; then
        copy_artifact_if_present "$after_nodestate" "${OUTPUT_DIR}/${label}_NodeState.csv"
    fi

    if [ -n "$after_migration" ]; then
        copy_artifact_if_present "$after_migration" "${OUTPUT_DIR}/${label}_MigrationInfo.csv"
    fi
}

run_set() {
    local set_name="$1"

    case "$set_name" in
        set1_percentile)
            echo "[Set 1] Forecast percentile"
            run_one "set1_percentile_p90" "${REPO_ROOT}/experiments/set1_percentile/input_base.txt" "0.9"
            run_one "set1_percentile_p75" "${REPO_ROOT}/experiments/set1_percentile/input_base.txt" "0.75"
            run_one "set1_percentile_p50" "${REPO_ROOT}/experiments/set1_percentile/input_base.txt" "0.5"
            run_one "set1_percentile_p25" "${REPO_ROOT}/experiments/set1_percentile/input_base.txt" "0.25"
            run_one "set1_percentile_p01" "${REPO_ROOT}/experiments/set1_percentile/input_base.txt" "0.01"
            ;;
        set2_alpha)
            echo "[Set 2] Skewness alpha"
            run_one "set2_alpha_input_alpha1" "${REPO_ROOT}/experiments/set2_alpha/input_alpha1.txt" "0.9"
            run_one "set2_alpha_input_alpha2" "${REPO_ROOT}/experiments/set2_alpha/input_alpha2.txt" "0.9"
            run_one "set2_alpha_input_alpha4" "${REPO_ROOT}/experiments/set2_alpha/input_alpha4.txt" "0.9"
            run_one "set2_alpha_input_alpha8" "${REPO_ROOT}/experiments/set2_alpha/input_alpha8.txt" "0.9"
            ;;
        set3_nodes)
            echo "[Set 3] Node count"
            run_one "set3_nodes_input_nodes196" "${REPO_ROOT}/experiments/set3_nodes/input_nodes196.txt" "0.9"
            run_one "set3_nodes_input_nodes176" "${REPO_ROOT}/experiments/set3_nodes/input_nodes176.txt" "0.9"
            run_one "set3_nodes_input_nodes157" "${REPO_ROOT}/experiments/set3_nodes/input_nodes157.txt" "0.9"
            run_one "set3_nodes_input_nodes137" "${REPO_ROOT}/experiments/set3_nodes/input_nodes137.txt" "0.9"
            run_one "set3_nodes_input_nodes127" "${REPO_ROOT}/experiments/set3_nodes/input_nodes127.txt" "0.9"
            ;;
        set4_forecast_window)
            echo "[Set 4] Forecast window"
            run_one "set4_forecast_window_input_window12" "${REPO_ROOT}/experiments/set4_forecast_window/input_window12.txt" "0.9"
            run_one "set4_forecast_window_input_window24" "${REPO_ROOT}/experiments/set4_forecast_window/input_window24.txt" "0.9"
            run_one "set4_forecast_window_input_window36" "${REPO_ROOT}/experiments/set4_forecast_window/input_window36.txt" "0.9"
            run_one "set4_forecast_window_input_window72" "${REPO_ROOT}/experiments/set4_forecast_window/input_window72.txt" "0.9"
            ;;
        *)
            echo "Error: unknown set '${set_name}'" >&2
            exit 1
            ;;
    esac
}

if [ "$#" -eq 0 ]; then
    RUN_SETS=("${DEFAULT_SETS[@]}")
else
    RUN_SETS=("$@")
fi

for set_name in "${RUN_SETS[@]}"; do
    valid=false
    for candidate in "${VALID_SETS[@]}"; do
        if [ "$set_name" = "$candidate" ]; then
            valid=true
            break
        fi
    done
    if [ "$valid" = false ]; then
        echo "Error: invalid set '${set_name}'" >&2
        echo "Valid sets: ${VALID_SETS[*]}" >&2
        exit 1
    fi
done

if [ ! -x "$POLICY_EXECUTOR" ]; then
    echo "Error: policy executor not found or not executable: $POLICY_EXECUTOR" >&2
    echo "Build it first with: cd build && bash make.sh" >&2
    exit 1
fi

require_file "$TESTBED"
mkdir -p "$OUTPUT_DIR"

echo "=============================================="
echo "LoadStar experiment batch runner"
echo "=============================================="
echo "Sets: ${RUN_SETS[*]}"
echo "Logs: ${OUTPUT_DIR}"
echo "=============================================="

for set_name in "${RUN_SETS[@]}"; do
    run_set "$set_name"
done

echo ""
echo "Completed."
echo "Experiment logs and copied artifacts are under experiments/outputs/"
