# Analyzing and Optimizing NoSQL Workloads for Cosmos DB using Distressed Resource Volume Metric
#### Gunika Verma<sup>1</sup>, Aashutosh A V<sup>1</sup>, Pooja Srinivas<sup>1</sup>, Yogesh Simmhan<sup>2</sup>, Ayush Choure<sup>1</sup>, Harshit Shah<sup>3</sup>, Mayukh Das<sup>1</sup>, Prashant Sasatte<sup>3</sup>, Chetan Bansal<sup>1</sup>, Abhijit Pai<sup>3</sup>, Suraj Dixit<sup>3</sup> and Achint Agrawal<sup>3</sup>
#### <sup>1</sup>M365 Research, Microsoft, Bangalore <sup>2</sup>Indian Institute of Science, Bangalore, <sup>3</sup>Azure, Microsoft, Bangalore
#### To Appear in [*PVLDB 2026*](https://www.vldb.org/pvldb/volumes/19/)


Large scale managed cloud databases leverage sophisticated load packing algorithms, which provide the efficiencies and economy of scale necessary for running these services. We address this in the context of *Cosmos DB*, Microsoft's flagship cloud-hosted NoSQL database, through a novel problem definition of balancing packing efficiency against user-centric reliability metrics. We first propose open-source NoSQL workloads from real Cosmos DB clusters, and analyze these workloads to derive a novel reliability metric, *Distressed Resource Volume (DRV)*.

We then develop an open-source policy simulation framework, *LOADSTAR*, powered by a nonparametric statistical model of estimating the QoS of real traffic patterns. We define a *resource optimization problem* for placing Cosmos DB replicas onto VM nodes, develop a forecasting model for future load distributions, *LUNA*, and a placement algorithm that uses these forecasts to trigger and rebalance stressed replicas, *ORBIT*, to reduce tail-errors. Our experiments demonstrate ORBIT's benefits over the existing Cosmos DB policy and a worst-fit optimized baseline, with higher load delivered at lower error rates and up to 35% reduction in resources.

---

## About this Repository
This repository contains the artifacts for the above VLDB 2026 article. Specifically, it includes the *Policy Simulator ([LOADSTAR](loadstar))* and the *Forecasting Model ([LUNA+ORBIT](luna))* used to evaluate the proposed models, policies and algorithms. Due to their large size, the *CosmosDB trace files* themselves for different testbeds used in the article are hosted on [Zenodo](https://zenodo.org/records/16539984) and accessed by our tools. We provide instructions for installing, building and running these components on a single machine.

These instructions help install the environment and run the experiments. The goal is ensure that the artifacts can be evaluated to be **Functional**, i.e., *the artifacts associated with the research are found to be documented, consistent, complete, exercisable, and include appropriate evidence of verification and validation.*

We provide scripts to (1) Setup the environment, (2) Build the code, (3) Process the CosmosDB trace data, (4) Run the LUNA+ORBIT forecasting model, (5) Run the LOADSTAR policy simulation, and (6) Analyze the performance using DRV profiles.

| Document | Description |
|----------|-------------|
| **[REPRODUCIBILITY.md](REPRODUCIBILITY.md)** | Quick-start here with a one-touch pipeline (for default and each experiments), expected artifacts, troubleshooting and manual steps. |
| **[experiments/README.md](experiments/README.md)** | Details of configurations required to run each experiment in the article. |
| **[DATASET.md](DATASET.md)** | Description of the CosmosDB replica trace files for testbeds, hosted on [Zenodo](https://doi.org/10.5281/zenodo.15564578) |
| **[INPUT_FORMAT.md](loadstar/Policy%20Executor/INPUT_FORMAT.md)** | Instructions for configuring and executing only the LoadStar policy simulator |
| **[INSTRUCTIONS.md](INSTRUCTIONS.md)** | Full setup, data flow, execution order, and analysis (DRV, error-rate preprocessing). |

---

## Acknowledgements
We thank students/staff from the [DREAM:Lab](https://dream-lab.in/), IISc, including Pranjal Naman, Mayank Arya, Kautuk Astu and Nikhil Reddy, for help with the experiments, plots and reproducibility.

## Contact

For more information on this repo, please contact the authors at Microsoft M365 Research (mayukhdas@microsoft.com).
