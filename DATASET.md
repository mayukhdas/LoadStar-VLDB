# Cosmos DB Trace Dataset

This describes the CosmosDB replica trace datasets and error rates for various *testbeds* used in the article for evaluation. Due to their large size (13.6GB) the datasets themseves are hosted on the [Zenodo archival site](https://zenodo.org/records/16539984) with DOI `10.5281/zenodo.15564578`.

## Files on Zenodo

| File(s) | Description |
|--------|-------------|
| **testbed_3march_9march_part1.xlsx** … **part29.xlsx** | Testbed traces used by this repo, as Excel sheets: resource usage per (Timestamp, PartitionId, UniqueId, ReplicaId) over a 7-day window (March 3–9, 2025). |
| **testbed_10march_16march_part1.xlsx** … **part29.xlsx** | Alternative testbed window (March 10–16, 2025), as Excel sheets; same schema as above. |
| **C2_SubReady.zip** | Preprocessed / submission-ready data for cluster C2. |
| **C3_SubReady.zip** | Preprocessed / submission-ready data for cluster C3. |
| **error_rates_2025-03-03_000000_2025-03-20_000000.xlsx** | Error-rate telemetry between March 3–20, 2025. |
| **error_rates_sample.xlsx** | Sample error-rate data. |


## Schema *(testbed CSVs)*

Our pipeline scripts download and combine the different parts and produce a single CSV file per testbed, without headers in the source files. The download script applies these column names:

| Column | Description |
|--------|-------------|
| **Timestamp** | Observation time (5-minute granularity in aggregated use). |
| **PartitionId** | Database partition (shard) identifier. |
| **UniqueId** | Unique identifier for the logical entity (e.g. partition view). |
| **ReplicaId** | Replica instance identifier (each partition has multiple replicas). |
| **sum_TotalRequestCharge** | Request Units (RUs) — load / request charge for the replica. |
| **MaxCounterCPU** | CPU usage (aggregate over cores). |
| **MaxCounterValueMemory** | Memory usage (e.g. in consistent units). |
| **TotalUsageInMB** | Storage usage in MB. |

Further, deduplication is applied on `(Timestamp, PartitionId, UniqueId)` (first row kept) before saving the final `testbed.csv`. This combined testbed file is then used by the Luna forecasting model and the LoadStar policy executor.
