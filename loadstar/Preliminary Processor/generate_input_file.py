import pandas as pd
import os 
import numpy as np

# Read the CSV file containing min and max timestamps
data=pd.read_csv('min_max_timestamps.csv',header=None)
data.columns=['PartitionId', 'ReplicaId','RoleInstance','min_TIMESTAMP','max_TIMESTAMP']
data['col_1'] = (data['min_TIMESTAMP'] != data['max_TIMESTAMP']).astype(int)
data= data[data['col_1']>0]
pd.to_datetime(data['min_TIMESTAMP'])
pd.to_datetime(data['max_TIMESTAMP'])

#Group by partitionId and sort by max_TIMESTAMP and min_TIMESTAMP
data = data.groupby('PartitionId', group_keys=False).apply(lambda x: x.sort_values('max_TIMESTAMP'), include_groups=False)
data = data.groupby('PartitionId', group_keys=False).apply(lambda x: x.sort_values('min_TIMESTAMP'), include_groups=False)
# Reset index so it is unique (avoids "Invalid call for scalar access" when using .at)
data = data.reset_index(drop=True)
data['UniqueId'] = np.nan

#Algorithm to create UniqueId for indentifying migrations from one node to another in same Partition using UniqueID

min_migration_count = float('inf')
max_migration_count = 0
min_partition_id = None
max_partition_id = None

grouped = data.groupby('PartitionId')
for name, group in grouped:
    i = 0
    endTime_map = dict()  
    for index, row in group.iterrows():
        if row['min_TIMESTAMP'] in endTime_map.values():
            matching_index = next((k for k, v in endTime_map.items() if v == row['min_TIMESTAMP']), None)
            data.at[index, 'UniqueId'] = data.at[matching_index, 'UniqueId']
            endTime_map[index] = row['max_TIMESTAMP']
            endTime_map.pop(matching_index)  
        else:
            data.at[index, 'UniqueId'] = i + 1
            endTime_map[index] = row['max_TIMESTAMP']
            i += 1
            
data = data.drop(columns=['col_1'])
df = data

# Convert string timestamps to datetime
df['min_TIMESTAMP'] = pd.to_datetime(df['min_TIMESTAMP'])
df['max_TIMESTAMP'] = pd.to_datetime(df['max_TIMESTAMP'])

# Create a new DataFrame with timestamps at every five-minute interval
new_rows = []

for index, row in df.iterrows():
    current_time = row['min_TIMESTAMP']
    end_time = row['max_TIMESTAMP']
    while current_time <= end_time:
        new_rows.append({'TIMESTAMP': current_time, 'PartitionId': row['PartitionId'], 'ReplicaId': row['ReplicaId'],'RoleInstance': row['RoleInstance'], 'UniqueId': row['UniqueId']})
        current_time += pd.Timedelta(minutes=5)

df1 = pd.DataFrame(new_rows)

# Define the directory where the .txt files are located
directory = 'RUs/downloads/C2_SubReady/RUs_2025-03-30_200000_2025-04-14_000000'

# Create an empty list to hold the dataframes
dfs = []

# RU .txt files: first line is header (space-separated), data rows are tab-separated with 11 columns.
# Skip header and assign column names after concat so downstream code (dropna, merge, groupby) works.
RU_COLUMNS = [
    'TIMESTAMP', 'PartitionId', 'ReplicaId', 'IsPrimary', 'NodeId',
    'sum_TotalRequestCharge', 'MaxCounterValueCPU', 'MaxCounterValueMemory',
    'TotalUsageInMB', 'readRqCount', 'writeRqCount'
]

for filename in os.listdir(directory):
    if filename.endswith('.txt'):
        file_path = os.path.join(directory, filename)
        print(file_path)
        df = pd.read_csv(file_path, sep="\t", header=None, skiprows=1, on_bad_lines='skip')
        dfs.append(df)

# Concatenate all DataFrames in the list
df2 = pd.concat(dfs, ignore_index=True)
df2.columns = RU_COLUMNS
df2 = df2.dropna(subset=['PartitionId'])
df2 = df2.reset_index(drop=True)
# Align types so merge succeeds (both sides string for PartitionId/ReplicaId)
df2['TIMESTAMP'] = pd.to_datetime(df2['TIMESTAMP'], utc=True)
df2['PartitionId'] = df2['PartitionId'].astype(str)
df2['ReplicaId'] = df2['ReplicaId'].astype(str)
df1['PartitionId'] = df1['PartitionId'].astype(str)
df1['ReplicaId'] = df1['ReplicaId'].astype(str)
# Merge df2 with df1 on columns PartitionId and ReplicaId
new_df = pd.merge(df2, df1, on=['TIMESTAMP','PartitionId', 'ReplicaId'], how='left')

# Fill UniqueId column based on conditions
new_df['UniqueId'] = new_df.apply(lambda row: row['UniqueId'] if pd.notnull(row['UniqueId']) else row['ReplicaId'], axis=1)
new_df['sum_TotalRequestCharge'] = new_df['sum_TotalRequestCharge'].astype(int)

# Aggregate and keep only columns expected by data_smoothening.py (luna)
result = new_df.groupby(['TIMESTAMP', 'PartitionId', 'UniqueId', 'IsPrimary'], as_index=False)[['ReplicaId', 'sum_TotalRequestCharge']].sum()
result = result[['TIMESTAMP', 'PartitionId', 'UniqueId', 'IsPrimary', 'ReplicaId', 'sum_TotalRequestCharge']]
result['sum_TotalRequestCharge'] = result['sum_TotalRequestCharge'].astype(int)
result['TIMESTAMP'] = pd.to_datetime(result['TIMESTAMP'])

result.to_csv("testbed.csv", index=False, header=None)