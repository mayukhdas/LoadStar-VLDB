#!/usr/bin/env python
# coding: utf-8

# In[13]:

# Ignore all warnings
import math
import warnings
warnings.filterwarnings("ignore")

import pandas as pd

import pandas as pd
import os

# Path to the folder containing CSVs, or single error_rates.csv next to this script
_script_dir = os.path.dirname(os.path.abspath(__file__))
folder_path = os.path.join(_script_dir, "error_rates")
df1_list = []
if os.path.isdir(folder_path):
    for file in os.listdir(folder_path):
        if file.endswith('.csv'):
            file_path = os.path.join(folder_path, file)
            try:
                df1_list.append(pd.read_csv(file_path))
            except Exception as e:
                print(f"Failed to read {file_path}: {e}")
# Fallback: single error_rates.csv in script directory (e.g. from pipeline)
if not df1_list:
    single_csv = os.path.join(_script_dir, "error_rates.csv")
    if os.path.isfile(single_csv):
        try:
            df1_list.append(pd.read_csv(single_csv))
        except Exception as e:
            print(f"Failed to read {single_csv}: {e}")
if not df1_list:
    df = pd.DataFrame(columns=['TotalReq', 'ErrorRate'])
else:
    df = pd.concat(df1_list, ignore_index=True)


if not df.empty and 'ErrorRate' in df.columns:
    df = df[df['ErrorRate']<=0.1]
# In[14]:


sorted_df = df.sort_values(by='TotalReq').reset_index(drop=True) if not df.empty and 'TotalReq' in df.columns else df.copy()


# In[15]:


num_bins = 100
if not sorted_df.empty and 'TotalReq' in sorted_df.columns:
    print(f"Number of bins: {num_bins}")
    # Create the bins ensuring equal counts in each bin
    bins = pd.qcut(sorted_df['TotalReq'], q=num_bins, labels=False, duplicates='drop')
    sorted_df['Bin'] = bins
else:
    sorted_df['Bin'] = []
    print("No error_rates data: using empty bins.")


# In[16]:


def store_bin_ranges(df):    
    df_sorted = df.sort_values(by='TotalReq')
    # Group the sorted DataFrame by 'Bin' and calculate the count of each value within the bin
    bin_counts = df_sorted.groupby('Bin')['ErrorRate'].value_counts()
    # Initialize an empty dictionary to store the bin data
    bins_data = {}

    # Store the count of each errorrate value within the bin in the dictionary
    for (bin_num, error_rate), count in bin_counts.items():
        if bin_num not in bins_data:
            bins_data[bin_num] = {'counts': {}}
        bins_data[bin_num]['counts'][error_rate] = count

    # Calculate and store the min and max based on TotalReq column for every bin
    sorted_bins = sorted(bins_data.keys())
    for i in range(len(sorted_bins)):
        bin_num = sorted_bins[i]
        bin_info = bins_data[bin_num]
        bin_values = list(bin_info['counts'].keys())
        min_value = min(bin_values)
        max_value = max(bin_values)
        
        # Make the values continuous by making max of previous bin as min of next bin
        if i > 0:
            prev_bin_num = sorted_bins[i - 1]
            bins_data[bin_num]['min'] = bins_data[prev_bin_num]['max']
        else:
            bins_data[bin_num]['min'] = 0

        # Set the maximum value of the last bin as max of the Totalreq value
        bins_data[bin_num]['max'] = int(df_sorted[df_sorted['Bin'] == bin_num]['TotalReq'].max())

    return bins_data

bins_data = store_bin_ranges(sorted_df)


# In[17]:


def find_bin(total_req, bin_ranges_dict):
    for bin_num, data in bin_ranges_dict.items():
        if data['min'] <= total_req <= data['max']:
            return bin_num  # Return the bin number if found
    
    return None

# Example:
total_req = 1000000  # Example TotalReq value
bin_num = find_bin(total_req, bins_data)
print(f"The TotalReq value {total_req} belongs to bin {bin_num}")
print(bins_data)

# In[18]:


import numpy as np

def predict_new_values(new_values, bins_data):
    predictions = []
    for new_value in new_values:
        # If total is 0, prediction is 0
        if new_value == 0:
            predictions.append(0) 
            continue
        
        bin_label = find_bin(int(new_value), bins_data)
        
        if bin_label is not None:
            bin_info = bins_data.get(bin_label)
            if bin_info is not None and 'counts' in bin_info and len(bin_info['counts']) > 0:
                
                # Extract values and their counts from the bin's data
                values = list(bin_info['counts'].keys())
                counts = list(bin_info['counts'].values())
                
                # Randomly pick a value from the values list with the corresponding counts as weights
                prediction = np.random.choice(values, p=counts/np.sum(counts))
                predictions.append(prediction)
            else:
                print(f"No data found for bin {bin_label}. Skipping prediction.")
        else:
            predictions.append(0) 
            continue
    return predictions



# In[19]:


import pandas as pd

def create_bins(data):
    df = pd.DataFrame(data)

    max_value_ErrorRate = 0.01
    min_value_ErrorRate = 0
    bin_boundaries = [min_value_ErrorRate + i * (max_value_ErrorRate / 9) for i in range(10)]
    bin_labels = list(range(1, 10))

    df['Bin'] = pd.cut(df['ErrorRate'], bins=bin_boundaries, labels=bin_labels, include_lowest=True, duplicates='drop')
    df['Bin'] = pd.to_numeric(df['Bin'], errors='coerce')

    # Assigning last bin (10) for values outside the range
    df['Bin'].fillna(10, inplace=True)  

    print(df)
    return df

def cumulative_sum_by_group(df):
    cumulative_sums = []
    
    for value in range(1, 12):
        group_df = df[df['Bin'] == float(value)]
        cumulative_sum = group_df['TotalReq'].sum() if not group_df.empty else 0.0
        cumulative_sums.append(cumulative_sum)
    
    return cumulative_sums

def cumulative_sum(arr):
    cum_sum = [arr[0]]
    for i in range(1, len(arr)):
        cum_sum.append(cum_sum[i-1] + arr[i])
    
    return cum_sum

import pandas as pd

def compute_ratio(df):
    total_multiplication = 0
    
    for index, row in df.iterrows():
        multiplication = row['ErrorRate'] * row['TotalReq']
        total_multiplication += multiplication
   
    sum_column = df['TotalReq'].sum()
    
    ratio = total_multiplication / sum_column
    return ratio


# In[25]:


import pandas as pd
import numpy as np
import matplotlib.pyplot as plt

def plot_drv_profiles(totalRU, totalRU1, totalRU2, totalRU3, totalRU4, totalRU5, actual_data, predicted_data1,predicted_data2,predicted_data3,predicted_data4, predicted_data5):
    # Create DataFrames for actual and predicted data
    actual_df = pd.DataFrame({'TotalReq': totalRU, 'ErrorRate': actual_data})
    predicted_df1 = pd.DataFrame({'TotalReq': totalRU1, 'ErrorRate': predicted_data1})
    predicted_df2 = pd.DataFrame({'TotalReq': totalRU2, 'ErrorRate': predicted_data2})
    predicted_df3 = pd.DataFrame({'TotalReq': totalRU3, 'ErrorRate': predicted_data3})
    predicted_df4 = pd.DataFrame({'TotalReq': totalRU4, 'ErrorRate': predicted_data4})
    predicted_df5 = pd.DataFrame({'TotalReq': totalRU5, 'ErrorRate': predicted_data5})
    
    # Binning the actual and predicted values 
    binned_actual_df = create_bins(actual_df)
    binned_predicted_df1 = create_bins(predicted_df1)
    binned_predicted_df2 = create_bins(predicted_df2)
    binned_predicted_df3 = create_bins(predicted_df3)
    binned_predicted_df4 = create_bins(predicted_df4)
    binned_predicted_df5 = create_bins(predicted_df5)
    
    sum_df = cumulative_sum_by_group(binned_actual_df)
    sum_df1 = cumulative_sum_by_group(binned_predicted_df1)
    sum_df2 = cumulative_sum_by_group(binned_predicted_df2)
    sum_df3 = cumulative_sum_by_group(binned_predicted_df3)
    sum_df4 = cumulative_sum_by_group(binned_predicted_df4)
    sum_df5 = cumulative_sum_by_group(binned_predicted_df5)
    
    print("BestFit:")
    print(sum_df)
    print("BestFit + 1hourForecasts:")
    print(sum_df1)
    print("WorstFit:")
    print(sum_df2)
    print("WorstFit + 1hourForecasts:")
    print(sum_df3)
    print("WorstFit + 2hourForecasts:")
    print(sum_df4)
    print("WorstFit + 3hourForecasts:")
    print(sum_df5)
    
    cumsum_df = cumulative_sum(sum_df)
    cumsum_df1 = cumulative_sum(sum_df1)
    cumsum_df2 = cumulative_sum(sum_df2)
    cumsum_df3 = cumulative_sum(sum_df3)
    cumsum_df4 = cumulative_sum(sum_df4)
    cumsum_df5 = cumulative_sum(sum_df5)

    # print("BestFit:",cumsum_df)
    # print("BestFit + 1hourForecasts:",cumsum_df1)
    # print("WorstFit:",cumsum_df2)
    # print("WorstFit + 1hourForecasts:",cumsum_df3)
    # print("WorstFit + 2hourForecasts:",cumsum_df4)
    # print("WorstFit + 3hourForecasts:",cumsum_df5)
    
    # Compute DRV index
    result= compute_ratio(binned_actual_df)
    result1 = compute_ratio(binned_predicted_df1)
    result2 = compute_ratio(binned_predicted_df2)
    result3 = compute_ratio(binned_predicted_df3)
    result4 = compute_ratio(binned_predicted_df4)
    result5 = compute_ratio(binned_predicted_df5)
    
    result = round(result, 5)
    result1 = round(result1, 5)
    result2 = round(result2, 5)
    result3 = round(result3, 5)
    result4 = round(result4, 5)
    result5 = round(result5, 5)
    
    x = np.arange(1, len(sum_df) + 1)
    colors = ['blue', 'red','green','orange', 'purple', 'brown']
    
    plt.figure(figsize=(10, 6))
    plt.plot(x, np.log(sum_df), color=colors[0], label=f'Best Fit (DRV:{result})')
    plt.plot(x, np.log(sum_df1), color=colors[1], label=f'Best Fit+ 1hr Forecasts (DRV:{result1})')
    plt.plot(x, np.log(sum_df2), color=colors[2], label=f'Worst Fit (DRV:{result2})')
    plt.plot(x, np.log(sum_df3), color=colors[3], label=f'Worst Fit+ 1hr Forecasts (DRV:{result3})')
    plt.plot(x, np.log(sum_df4), color=colors[4], label=f'Worst Fit+ 2hr Forecasts (DRV:{result4})')
    plt.plot(x, np.log(sum_df5), color=colors[5], label=f'Worst Fit+ 3hr Forecasts (DRV:{result5})')
    
    plt.title('DRV Profiles')
    plt.xlabel('Bins of ErrorRate')
    plt.ylabel('Logarithm of TotalReq')
    plt.legend()
    plt.grid(True)
    plt.show()



# In[21]:


def plot_drv_profiles_scale_tests(totalRU, totalRU1, totalRU2, actual_data, predicted_data1,predicted_data2):
    # Create DataFrames for actual and predicted data
    actual_df = pd.DataFrame({'TotalReq': totalRU, 'ErrorRate': actual_data})
    predicted_df1 = pd.DataFrame({'TotalReq': totalRU1, 'ErrorRate': predicted_data1})
    predicted_df2 = pd.DataFrame({'TotalReq': totalRU2, 'ErrorRate': predicted_data2})
    
    # Binning the actual and predicted values 
    binned_actual_df = create_bins(actual_df)
    binned_predicted_df1 = create_bins(predicted_df1)
    binned_predicted_df2 = create_bins(predicted_df2)
    
    sum_df = cumulative_sum_by_group(binned_actual_df)
    sum_df1 = cumulative_sum_by_group(binned_predicted_df1)
    sum_df2 = cumulative_sum_by_group(binned_predicted_df2)
    
    cumsum_df = cumulative_sum(sum_df)
    cumsum_df1 = cumulative_sum(sum_df1)
    cumsum_df2 = cumulative_sum(sum_df2)

    print(cumsum_df)
    print(cumsum_df1)
    print(cumsum_df2)
    
    # Compute DRV index
    result= compute_ratio(binned_actual_df)
    result1 = compute_ratio(binned_predicted_df1)
    result2 = compute_ratio(binned_predicted_df2)
    result = round(result, 5)
    result1 = round(result1, 5)
    result2 = round(result2, 5)
    
    x = np.arange(1, len(sum_df) + 1)
    colors = ['blue', 'red','green']
    
    plt.figure(figsize=(10, 6))
    plt.plot(x, np.log(sum_df), color=colors[0], label=f'Simulated Annealing (DRV:{result})')
    plt.plot(x, np.log(sum_df1), color=colors[1], label=f'Best First with binary (DRV:{result1})')
    plt.plot(x, np.log(sum_df2), color=colors[2], label=f'Worst First with binary (DRV:{result2})')
    
    plt.title('DRV Profiles')
    plt.xlabel('Bins of ErrorRate')
    plt.ylabel('Logarithm of TotalReq')
    plt.legend()
    plt.grid(True)
    plt.show()


# In[22]:
def plot_drv_profiles_sample_plots(totalRU, totalRU1, actual_data, predicted_data1):
    # Create DataFrames for actual and predicted data
    actual_df = pd.DataFrame({'TotalReq': totalRU, 'ErrorRate': actual_data})
    predicted_df1 = pd.DataFrame({'TotalReq': totalRU1, 'ErrorRate': predicted_data1})
    
    # Binning the actual and predicted values 
    binned_actual_df = create_bins(actual_df)
    binned_predicted_df1 = create_bins(predicted_df1)
    
    sum_df =cumulative_sum_by_group(binned_actual_df)
    sum_df1 = cumulative_sum_by_group(binned_predicted_df1)
    
    print(sum_df)
    print(sum_df1)
    
    cumsum_df = cumulative_sum(sum_df)
    cumsum_df1 = cumulative_sum(sum_df1)

    print(cumsum_df)
    print(cumsum_df1)
    
    # Compute DRV index
    result= compute_ratio(binned_actual_df)
    result1 = compute_ratio(binned_predicted_df1)
    result = round(result, 5)
    result1 = round(result1, 5)
    
    x = np.arange(1, len(sum_df) + 1)
    colors = ['blue', 'red']
    
    plt.figure(figsize=(10, 6))
    plt.plot(x, np.log(sum_df), color=colors[0], label=f'Policy 1 (DRV:{result})')
    plt.plot(x, np.log(sum_df1), color=colors[1], label=f'Policy 2 (DRV:{result1})')
    
    plt.title('DRV Profiles')
    plt.xlabel('Bins of ErrorRate')
    plt.ylabel('Logarithm of TotalReq')
    plt.legend()
    plt.grid(True)
    plt.show()


# In[ ]:




