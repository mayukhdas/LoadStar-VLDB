#pragma once
#include <cmath>
#include <iostream>
#include <map>
#include <vector>
#include <set>
#include <string>
#include "NodeLoadReplica.hpp"
#include <iomanip>
#include <fstream>
#include "../common/common.h"
#include <boost/multiprecision/cpp_int.hpp>
#include "d-RU.hpp"
#include "../fileReader/FileReader.hpp"
#include "ForecastValue.hpp"
namespace boost
{
    using int512_t = boost::multiprecision::int512_t;
}
using namespace std;

// This class is used to store the state of the system at a specific time in the simulation.
class CurrentState
{
    // Function to print the aggregated values of load, cpu, memory, stoarge of nodes at each timestamp as output file (NodeStateFile)
    void PrintNodeState(char *NodeStateFile)
    {
        std::ofstream myfile;
        myfile.open(NodeStateFile, std::ios_base::app);
        myfile << std::fixed << setprecision(8) << endl;
        for (int i = 0; i < NODE_COUNT; i++)
        {
            // Adding Time,NodeId,CurrLoad,CurrCpu,CurrMemory,CurrStorage
            myfile << time << "," << i << "," << NodeIdToNodeLoadMap[i] << " " << NodeIdToNodeCPUMap[i] << " " << NodeIdToNodeMemoryMap[i] << " " << NodeIdToNodeStorageMap[i] << endl;
        }
        myfile.close();
    }

    // Function to print the state of each replica at every timestamp of simulation as output file (ReplicaStateFile)
    void PrintReplicaState(char *ReplicaStateFile)
    {
        std::ofstream myfile2;
        myfile2.open(ReplicaStateFile, std::ios_base::app);
        for (auto it : uriParitionPlacementMap)
        {
            // Adding Time,UniqueReplicaId,PartitionId,NodeId,Load,ReplicaId
            myfile2 << time << "," << it.first.first << "," << it.first.second << "," << it.second.NodeId << "," << it.second.Load << "," << NodeIdToNodeLoadMap[it.second.NodeId] << "," << it.second.ReplicaId << endl;
        }
        myfile2.close();
    }

public:
    // maps <UniqueReplicaId, PartitionId> to current node on which replica is running and its load requirement and replicaId
    map<pair<string, string>, NodeLoadReplica> uriParitionPlacementMap;

    // stores current timestamp
    string time;

    // maps NodeId to set of <UniqueReplicaId, PartitionId> running at that particular node
    map<int, set<pair<string, string>>> NodeIdToUriPartitionMap;

    // maps every NodeId to aggregrated values of Load, CPU, Memory, Storage at that particular node
    map<int, boost::int512_t> NodeIdToNodeLoadMap;
    map<int, boost::int512_t> NodeIdToNodeCPUMap;
    map<int, boost::int512_t> NodeIdToNodeMemoryMap;
    map<int, boost::int512_t> NodeIdToNodeStorageMap;

    // thi map store ru values for uri and partition
    map<pair<string, string>, Ru> uriPartitionRuMap;

    // the map stores forecast values for next FORECAST_WINDOW_SIZE timestamps at time t for <UniqueReplicaId, PartitionId> (used when only replica level load forecats is used)
    static map<string, map<pair<string, string>, boost::int512_t>> timeURIPartitionForecastsMap;

    // the map stores timestamp to <PartitionId,Forecasts for next FORECAST_WINDOW_SIZE timestamps for all dimensions>
    static map<string, map<string, ForecastValue>> timeURIPartitionForecastsMap4Dim;

    // Maps every PartitionId to its class for compatibility matrix scoring
    static map<string, int> uriPartitionClassMap;

    CurrentState(string time) : time(time) {}

    // Function to store load forecasts for each timestamp and each <UniqueReplicaId, PartitionId> pair
    static void generate_forecast_map(map<string, vector<Row>> &TableGroupedByTime)
    {
        for (const auto &timeEntry : TableGroupedByTime)
        {
            string currentTime = timeEntry.first;
            const auto &rows = timeEntry.second;
            for (const auto &row : rows)
            {
                string uri = row.UniqueReplicaId;
                string partition = row.partitionId;
                boost::int512_t forecastValue = row.LoadForecasts;
                timeURIPartitionForecastsMap[currentTime][{uri, partition}] = forecastValue;
            }
        }
    }

    // Function to store the d-forecasts for each PartitionId at every timestamp
    static void generate_forecast_mapV2(map<string, vector<Row>> &TableGroupedByTime)
    {
        for (const auto &timeEntry : TableGroupedByTime)
        {
            string currentTime = timeEntry.first;
            const auto &rows = timeEntry.second;
            for (const auto &row : rows)
            {
                string uri = row.UniqueReplicaId;
                string partition = row.partitionId;
                timeURIPartitionForecastsMap4Dim[currentTime][partition] = ForecastValue(row.LoadForecasts, row.CPUForecasts, row.MemoryForecasts, row.StorageForecasts);
            }
        }
    }

    // Function to print the NodeState and ReplicaState output files
    void Print(char *NodeStateFile, char *ReplicaStateFile)
    {
        if (time == "")
        {
            return;
        }

        PrintNodeState(NodeStateFile);

        // commented the code for ReplicaStateFile
        // PrintReplicaState(ReplicaStateFile);
    }

    // Function to update the maps when some replicas are completed
    void DeleteReplica(int nodeId, string uri, string partition)
    {

        boost::int512_t load = uriParitionPlacementMap[{uri, partition}].Load;
        boost::int512_t cpu = uriParitionPlacementMap[{uri, partition}].CPU;
        boost::int512_t memory = uriParitionPlacementMap[{uri, partition}].Memory;
        boost::int512_t storage = uriParitionPlacementMap[{uri, partition}].Storage;
        NodeIdToNodeLoadMap[nodeId] -= load;
        NodeIdToNodeCPUMap[nodeId] -= cpu;
        NodeIdToNodeMemoryMap[nodeId] -= memory;
        NodeIdToNodeStorageMap[nodeId] -= storage;
        NodeIdToUriPartitionMap[nodeId].erase({uri, partition});
        uriParitionPlacementMap.erase({uri, partition});
        uriPartitionRuMap.erase({uri, partition});
    }
    void UpdateReplica(pair<string, string> uriPartition, LoadReplica incomingLoadReplica, Ru incomingRu, NodeLoadReplica previousNodeLoadReplica, Ru previousRu)
    {
        uriParitionPlacementMap[uriPartition].Load = incomingLoadReplica.Load;
        uriParitionPlacementMap[uriPartition].CPU = incomingRu.Cpu;
        uriParitionPlacementMap[uriPartition].Memory = incomingRu.Memory;
        uriParitionPlacementMap[uriPartition].Storage = incomingRu.Storage;

        NodeIdToNodeLoadMap[previousNodeLoadReplica.NodeId] += incomingLoadReplica.Load - previousNodeLoadReplica.Load;
        NodeIdToNodeCPUMap[previousNodeLoadReplica.NodeId] -= (previousRu.Cpu - incomingRu.Cpu);
        NodeIdToNodeMemoryMap[previousNodeLoadReplica.NodeId] -= (previousRu.Memory - incomingRu.Memory);
        NodeIdToNodeStorageMap[previousNodeLoadReplica.NodeId] -= (previousRu.Storage - incomingRu.Storage);
        uriPartitionRuMap[uriPartition] = incomingRu;
    }

    // Function to generate the uriPartitionClassMap from the csv file
    static void generate_uri_partition_class_map(fstream &uriPartitionClasses)
    {
        vector<string> row;
        string line, word;
        while (getline(uriPartitionClasses, line))
        {
            row.clear();
            stringstream str(line);
            while (getline(str, word, ','))
                row.push_back(word);
            string partitionId = row[0];
            int Class = stoi(row[1]);
            uriPartitionClassMap[partitionId] = Class;
        }
    }

    // Function to generate the d-forecast map for each PartitionId for each timestamp
    static void generate_uri_partition_ForecastRU_map(fstream &PartitionForecast, string RUtype)
    {
        vector<string> row;
        string line, word;
        while (getline(PartitionForecast, line))
        {
            row.clear();
            stringstream str(line);
            while (getline(str, word, ','))
                row.push_back(word);
            string time = row[0];
            string partitionId = row[1];
            // Accept both int and float in forecast CSV; convert to int512_t
            boost::int512_t forecast = static_cast<long long>(std::round(std::stod(row[2])));
            if (RUtype == "load")
            {
                timeURIPartitionForecastsMap4Dim[time][partitionId].ForecastLoad = max(timeURIPartitionForecastsMap4Dim[time][partitionId].ForecastLoad, forecast);
            }
            if (RUtype == "cpu")
            {
                timeURIPartitionForecastsMap4Dim[time][partitionId].ForecastCPU = max(timeURIPartitionForecastsMap4Dim[time][partitionId].ForecastCPU, forecast);
            }
            if (RUtype == "memory")
            {
                timeURIPartitionForecastsMap4Dim[time][partitionId].ForecastMemory = max(timeURIPartitionForecastsMap4Dim[time][partitionId].ForecastMemory, forecast);
            }
            if (RUtype == "storage")
            {
                timeURIPartitionForecastsMap4Dim[time][partitionId].ForecastStorage = max(timeURIPartitionForecastsMap4Dim[time][partitionId].ForecastStorage, forecast);
            }
        }
    }
};