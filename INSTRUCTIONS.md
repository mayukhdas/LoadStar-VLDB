# LOADSTAR & LUNA-ORBIT: Policy Simulation and Forecasting Framework

A comprehensive framework for distributed system analysis combining policy simulation (LOADSTAR) and time series forecasting (LUNA-ORBIT).

## 📁 Project Structure

```
source/
├── LOADSTAR [policy_simulator]/     # Policy simulation and analysis
│   ├── ErrorRate Load_Relationships/
│   │   └── error_rates_kde_preprocessing.ipynb
│   ├── Metric Processor/
│   │   ├── drv_profiles_comparisons.py
│   │   └── generate_drv_profiles.py
│   ├── Policy Executor/
│   │   ├── INPUT_FORMAT.md
│   │   ├── main.cpp
│   │   └── [C++ header files]
│   └── Preliminary Processor/
│       └── generate_input_file.py
├── LUNA-ORBIT [forecasting_model]/  # Time series forecasting
│   ├── data_smoothening.py
│   ├── forecasting_model.py
│   └── expand_and_save_forecasts.py
├── requirements.txt
├── setup.sh
├── REPRODUCIBILITY.md
├── DATASET.md
├── INSTRUCTIONS.md
└── README.md
```

## 🎯 System Overview

### LOADSTAR [Policy Simulator]
Simulates distributed system load balancing policies and analyzes performance metrics including error rates and resource utilization.

### LUNA-ORBIT [Forecasting Model]
Provides time series forecasting using LightGBM with quantile regression for predicting future system loads.

## 🚀 Complete Setup and Execution Guide

### Step 1: Environment Setup

#### 1.1 Prerequisites
- Python 3.8+
- C++ compiler (g++)
- Git (for cloning)

#### 1.2 Initial Setup
```bash
# Clone the repository
git clone <repository-url>
cd source

# Make setup script executable
chmod +x setup.sh

# Run setup script (creates virtual environment and installs dependencies)
./setup.sh
```

#### 1.3 Manual Setup (Alternative)
```bash
# Create virtual environment
python3 -m venv venv

# Activate virtual environment
source venv/bin/activate  # On Windows: venv\Scripts\activate

# Install dependencies
pip install -r requirements.txt

# Create output directories
mkdir -p outputs
mkdir -p "LOADSTAR [policy_simulator]/outputs"
mkdir -p "LUNA-ORBIT [forecasting_model]/outputs"
```

### Step 2: Compile C++ Components

```bash
cd "LOADSTAR [policy_simulator]/Policy Executor"
g++ -std=c++17 -O2 -o policy_executor main.cpp
cd ../..
```

## 📊 Data Flow and Execution Order

### Phase 1: Data Preprocessing

#### 1.1 Prepare Raw Data
**Input Required**: Raw system metrics data with the following structure:

**File**: `min_max_timestamps.csv`
```csv
PartitionId,ReplicaId,RoleInstance,min_TIMESTAMP,max_TIMESTAMP
P001,R001,Instance1,2025-03-01 00:00:00,2025-03-01 23:55:00
P002,R002,Instance2,2025-03-01 00:05:00,2025-03-01 23:50:00
```

**Directory**: `RUs/downloads/` containing `.txt` files with tab-separated values:
```
TIMESTAMP	PartitionId	ReplicaId	sum_TotalRequestCharge	MaxCounterValueCPU	MaxCounterValueMemory	readRqCount	writeRqCount	TotalUsageInMB
2025-03-01 00:00:00	P001	R001	1500	45.5	2048	100	50	1024
```

#### 1.2 Run Preliminary Processor
```bash
cd "LOADSTAR [policy_simulator]/Preliminary Processor"
python generate_input_file.py
```

**Output**: `testbed.csv` (headerless CSV)
```csv
2025-03-01 00:00:00,P001,U001,R001,1500,4550000,2,1024
2025-03-01 00:05:00,P001,U001,R001,1600,4600000,2.1,1050
```

**Columns**: `TIMESTAMP,PartitionId,UniqueId,ReplicaId,sum_TotalRequestCharge,MaxCounterValueCPU,MaxCounterValueMemory,TotalUsageInMB`

### Phase 2: LUNA-ORBIT Forecasting

#### 2.1 Data Smoothening
```bash
cd "LUNA-ORBIT [forecasting_model]"
python data_smoothening.py
```

**Input**: `input.csv` (headerless)
```csv
2025-03-01 00:00:00,P001,U001,1,R001,1500
2025-03-01 00:05:00,P001,U001,1,R001,1600
```

**Columns**: `TIMESTAMP,PartitionId,UniqueId,IsPrimary,ReplicaId,sum_TotalRequestCharge`

**Output**: `output.csv` with smoothed load values
```csv
TIMESTAMP,PartitionId,UniqueId,IsPrimary,ReplicaId,sum_TotalRequestCharge,Smoothed_Load
2025-03-01 00:00:00,P001,U001,1,R001,1500,1485.2
```

#### 2.2 Forecasting Model Training and Prediction

**For Load Forecasting**:
```bash
python forecasting_model.py --input input.csv --output_dir ./outputs --forecast_days 6 --quantiles 0.001 0.25 0.5 0.75 0.9 0.995 --n_iter 10 --cv 2
```

**For CPU/Memory/Storage Forecasting**:
To generate forecasts for all resource dimensions (required for policy type 9), you must run the forecasting model separately for each resource type by modifying the target column in the code:

```bash
# Modify forecasting_model.py to change target_col from 'NextDay_Load' to:
# - 'CPU' for CPU forecasting
# - 'Memory' for memory forecasting  
# - 'Storage' for storage forecasting

# Run for each resource type:
python forecasting_model.py --input input_cpu.csv --output_dir ./outputs_cpu
python forecasting_model.py --input input_memory.csv --output_dir ./outputs_memory
python forecasting_model.py --input input_storage.csv --output_dir ./outputs_storage
```

**Note**: For the recommended policy type 9, you need forecast files for all 4 dimensions (load, CPU, memory, storage). Each requires separate model training with appropriate input data and target columns.

**Parameters**:
- `--input`: Input CSV file path
- `--output_dir`: Output directory (default: ./outputs)
- `--quantiles`: Quantiles to predict (default: [0.001, 0.25, 0.5, 0.75, 0.9, 0.995])
- `--n_iter`: Hyperparameter tuning iterations (default: 10)
- `--cv`: Cross-validation folds (default: 2)
- `--forecast_days`: Number of days to forecast (default: 6)

**Outputs**:
- `predictions_by_timestamp_partition.csv`: Detailed predictions with columns:
  ```csv
  TIMESTAMP,PartitionId,Actual_load,pred_load_0,pred_load_25,pred_load_50,pred_load_75,pred_load_90,pred_load_99,forecast_date,prediction_day_index
  ```
- `predictions_pivot.csv`: Pivot table format
- Model files: `model_q{quantile}.pkl`
- Feature importance plots: `feature_importance_q{quantile}.png`
- Evaluation results: `evaluation_results.txt`

#### 2.3 Forecast Expansion
```bash
python expand_and_save_forecasts.py
```

**Output**: `forecast_0.9.csv` (headerless, UTC timestamps)
```csv
2025-03-02 00:15:00+00:00,P001,1650
2025-03-02 00:20:00+00:00,P001,1650
```

**Columns**: `TIMESTAMP,PartitionId,pred_load_50`

### Phase 3: Error Rate Analysis

#### 3.1 Error Rate KDE Preprocessing
```bash
cd "../LOADSTAR [policy_simulator]/ErrorRate Load_Relationships"
jupyter notebook error_rates_kde_preprocessing.ipynb
```

**Input**: `error_rates.csv`
```csv
TotalReq,ErrorRate
1500000,0.0001
1600000,0.0002
```

**Output**: Processed bins data structure for error rate prediction

### Phase 4: Policy Simulation

#### 4.1 Policy Execution
```bash
cd "../Policy Executor"
./policy_executor <testbed_filename> <num_args> [forecast_files...] < input.txt > output.txt
```

**Command Parameters**:
- `<testbed_filename>`: Input CSV file name (without .csv extension)
- `<num_args>`: Number of arguments (6 for CPU/Memory, 5 for load forecasts, 10 for all dimensions)
- `[forecast_files...]`: Forecast files based on num_args
- Input configuration is read from stdin (redirect from input.txt)
- Output is written to stdout (redirect to output.txt)

**Example Commands**:
```bash
# For recommended policy type 9 with all dimension forecasts (10 args)
./policy_executor testbed 10 cpu_forecast.csv load_forecast.csv memory_forecast.csv storage_forecast.csv < input.txt > output.txt
```

**Input Configuration File** (`input.txt`):
```
1.0
5000000
10
5000000
6000000
3
2000
Y
4500000
1
1
```

**Values in order**:
1. Skewness parameter (e.g., 1.0)
2. Load capacity of node (e.g., 5000000)
3. Number of nodes (e.g., 10)
4. Hard load capacity of node (e.g., 5000000)
5. CPU threshold (e.g., 6000000)
6. Memory threshold (e.g., 3)
7. Storage threshold (e.g., 2000)
8. Use soft load capacity? (Y/N)
9. Soft load capacity (if Y above, e.g., 4500000)
10. Policy type (9 - Recommended)
11. Forecast window size (e.g., 12)

**Recommended Policy Type**: 9 (WorstFitWithCPUMemoryRUPrediction)

**Policy Types**:
- 9: WorstFitWithCPUMemoryRUPrediction (Recommended)

**Migration Strategies**:
- 1: Binary selection
- 2: Lightest replica selection
- 3: Heaviest replica selection

**Note**: For the recommended policy type 9, you'll be prompted for forecast window size instead of migration strategy.

**Outputs**:
- `MigrationInfo.csv`: Migration records
  ```csv
  Time,UniqueReplicaId,PartitionId,OldNodeId,NewNodeId,ReplicaId
  2025-03-01 00:05:00,U001,P001,N001,N002,R001
  ```
- `{prefix}_NodeState.csv`: Node states at each timestamp (filename includes configuration parameters)
  ```csv
  Time,NodeId,CurrLoad,CurrCpu,CurrMemory,CurrStorage
  2025-03-01 00:00:00,N001,1500,4550000,2,1024
  ```

### Phase 5: Performance Analysis

#### 5.1 DRV Profile Generation
```bash
cd "../Metric Processor"
python drv_profiles_comparisons.py
```

**Inputs**:
- `error_rates.csv`: Original error rate data
- `{prefix}_NodeState.csv`: Simulation output from Policy Executor (filename varies based on configuration)

**Outputs**:
- DRV (Demand-Resource-Violation) profile plots
- Performance comparison metrics
- Console output with DRV indices for different policies

## 📋 Complete Execution Checklist

### Prerequisites Setup
- [ ] Python 3.8+ installed
- [ ] C++ compiler (g++) available
- [ ] Virtual environment created and activated
- [ ] Dependencies installed from requirements.txt

### Data Preparation
- [ ] `min_max_timestamps.csv` prepared
- [ ] Raw data files in `RUs/downloads/` directory
- [ ] `error_rates.csv` available for error rate analysis

### Execution Order
1. [ ] **Preliminary Processing**: Generate `testbed.csv`
2. [ ] **LUNA-ORBIT Smoothening**: Create smoothed data for all resource types
3. [ ] **LUNA-ORBIT Forecasting**: Generate forecast files for load, CPU, memory, and storage
4. [ ] **LUNA-ORBIT Expansion**: Create forecast files for all dimensions
5. [ ] **Error Rate Analysis**: Process KDE preprocessing
6. [ ] **Policy Simulation**: Run C++ policy executor with policy type 9
7. [ ] **Performance Analysis**: Generate DRV profiles

## 🔧 Configuration Files

### System Configuration (`input.txt`)
```
1.0        # Skewness parameter
5000000    # Load capacity of node
10         # Number of nodes
5000000    # Hard load capacity of node
6000000    # CPU threshold
3          # Memory threshold (GB)
2000       # Storage threshold (MB)
Y          # Use soft load capacity? (Y/N)
4500000    # Soft load capacity (if Y above)
9          # Policy type (9 - Recommended)
12         # Forecast window size
```

**Note**: The program reads these values sequentially through interactive prompts. Each line should contain only the numeric value or Y/N response. The configuration is read from stdin, so redirect from a file using `< input.txt`.

## 📈 Key Metrics and Analysis

### DRV (Demand-Resource-Violation) Profiles
- Measures system performance across different error rate bins
- Compares multiple policy strategies
- Provides quantitative performance metrics

### Forecasting Accuracy
- Multi-quantile predictions (0.1% to 99.5%)
- Feature importance analysis
- Cross-validation performance metrics

## 🔍 Troubleshooting

### Common Issues
1. **Missing Dependencies**: Ensure all packages in requirements.txt are installed
2. **C++ Compilation Errors**: Check g++ version and C++17 support
3. **Data Format Issues**: Verify CSV headers and data types match specifications
4. **Memory Issues**: Monitor system memory during large dataset processing

### Debug Tips
- Check log files for detailed error messages
- Verify input file formats match specifications exactly
- Ensure all forecast files are in the correct directory
- Monitor memory usage during parallel processing

## 📊 Expected Results

After successful execution, you should have:
- Forecast files for 6 days ahead
- Policy simulation results showing node states over time
- DRV profile comparisons between different policies
- Performance metrics and visualizations

## 🤝 Support

For issues or questions:
1. Check input data formats match specifications
2. Verify all dependencies are installed correctly
3. Review log files for error details
4. Ensure sufficient system memory for large datasets

---

*This framework enables comprehensive analysis of distributed system performance through advanced forecasting and policy simulation.*
