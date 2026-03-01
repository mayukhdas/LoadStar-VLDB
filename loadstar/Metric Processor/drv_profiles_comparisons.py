from generate_drv_profiles import *

import os
import pandas as pd
import numpy as np
from datetime import timedelta

# Load the actual data
actual_df= pd.read_csv("error_rates.csv")
actual_df.rename(columns={'CurrLoad': 'TotalReq'}, inplace=True)
totalRU = actual_df['TotalReq']
actual_data= actual_df['ErrorRate']


#simulated output trace (*NodeState.csv) from Policy Executor
df1 = pd.read_csv(
    "NodeState.csv" 
)

# Drop unwanted columns
df1 = df1.drop(columns=['CurrCpu', 'CurrMemory', 'CurrStorage'])
df1.rename(columns={'CurrLoad': 'TotalReq'}, inplace=True)
df1['TotalReq'] = df1['TotalReq'].astype(int)
df1['ErrorRate'] = predict_new_values(df1['TotalReq'],bins_data)

totalRU1 = df1['TotalReq']
predicted_data1 = df1['ErrorRate']

plot_drv_profiles_sample_plots(totalRU, totalRU1, actual_data, predicted_data1)