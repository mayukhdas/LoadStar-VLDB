import pandas as pd
import numpy as np
import torch

def whittaker_eilers_smoothing_torch(y, lambda_param):
    """Apply Whittaker-Eilers smoothing using PyTorch."""
    n = len(y)
    # Convert data to PyTorch tensors
    y_torch = torch.tensor(y, dtype=torch.float32)
    A = torch.zeros((n, n), dtype=torch.float32)
    b = torch.zeros(n, dtype=torch.float32)

    # Construct A matrix
    A[0, 0] = 1
    A[n-1, n-1] = 1
    for i in range(1, n-1):
        A[i, i-1] = 1
        A[i, i] = -2 - lambda_param
        A[i, i+1] = 1

    # Construct b vector
    b[1:-1] = -y_torch[1:-1] * lambda_param
    
    # Solve the linear system A @ smoothed = b
    smoothed_torch = torch.linalg.solve(A, b)
    
    # Convert result back to numpy array
    smoothed = smoothed_torch.numpy()
    return smoothed

def apply_smoothing(filtered_df, lambda_param=100):
    """Apply Whittaker-Eilers smoothing to each time series.""" 
    continuous_groups_df = pd.DataFrame(columns=filtered_df.columns)
    continuous_groups_df['Smoothed_Load'] = np.nan
    continuous_groups_df = continuous_groups_df.set_index(['TIMESTAMP', 'PartitionId', 'UniqueId'])

    for unique_id, group in filtered_df.groupby(['PartitionId', 'UniqueId']):
        # Only apply smoothing if the series has exactly 288 data points
            series = group['sum_TotalRequestCharge'].values  # Use 'sum_TotalRequestCharge' as 'Load'
            series = np.nan_to_num(series, nan=0.0).astype(np.float32)

            # Apply Whittaker-Eilers smoothing
            smoothed_series = whittaker_eilers_smoothing_torch(series, lambda_param)
            
            # Add the smoothed values to the group DataFrame
            group['Smoothed_Load'] = smoothed_series

            # Append the group with the smoothed column to the continuous_groups_df
            continuous_groups_df = pd.concat([continuous_groups_df, group])

    # Reset index to restore the original structure
    continuous_groups_df.reset_index(drop=True, inplace=True)

    return continuous_groups_df

def main():
    # Load data
    input_file_path = "input.csv" 
    filtered_df = pd.read_csv(input_file_path, header=None)
    filtered_df.columns = ['TIMESTAMP', 'PartitionId', 'UniqueId', 'IsPrimary','ReplicaId', 
                            'sum_TotalRequestCharge']

    print(filtered_df.head)
    max_length = filtered_df.groupby(['PartitionId', 'UniqueId']).size().max()
    print(max_length)
    # Apply smoothing
    continuous_groups_df = apply_smoothing(filtered_df, lambda_param=100)
    
    # Print or return the result
    print(continuous_groups_df)
    
    continuous_groups_df.to_csv('output.csv', index=None)

if __name__ == "__main__":
    main()
