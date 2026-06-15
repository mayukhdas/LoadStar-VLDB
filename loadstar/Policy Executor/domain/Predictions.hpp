#pragma once
#include <bits/stdc++.h>
#include "ForecastValue.hpp"
#include "MaxForecast.hpp"
#include "CurrentState.hpp"

using namespace std;

/* 
 This class is used to represent the predictions of resource utilization (RU) for a node.
 It contains a map that associates each node ID with a deque of aggregated forecast values and their corresponding timestamps.
 The class provides methods to add and delete replicas from the forecast map, find the maximum forecast values,
 and update the forecast map based on the current state of the system.
*/
class Predictions
{
public:

    /* 
    ForecastMap (<nodeId, <ForecastValue, time>>) is a map that associates each node ID with a deque of forecast values and their corresponding timestamps.
    ForecastValue is a struct that contains the forecasted values for CPU, Memory, Storage, and Load.
    time is a string that represents the time at which the forecast was made.
    The deque is used to maintain a fixed size of FORECAST_WINDOW_SIZE, where FORECAST_WINDOW_SIZE is the number of timestamps in future to be considered by policy.
    */
    map<int, deque<pair<ForecastValue,string>>> ForecastMap;
    int FORECAST_WINDOW_SIZE;
    Predictions(int FORECAST_WINDOW_SIZE) : FORECAST_WINDOW_SIZE(FORECAST_WINDOW_SIZE) {}
    Predictions() {}

    /*
    This method adds a replica to the forecast map for a given node ID and URI partition.
    It retrieves the forecasted values for the specified URI partition from the current state and updates the forecast map accordingly.
    */
    void AddReplicaToForecastMap(int nodeId, pair<string, string> uriPartition, CurrentState &currentState)
    {
        auto it = CurrentState::timeURIPartitionForecastsMap4Dim.find(currentState.time);
        if (it == CurrentState::timeURIPartitionForecastsMap4Dim.end())
        {
            return;
        }
        //assertion to make sure that the size of the forecast map for the node ID is equal to FORECAST_WINDOW_SIZE always
        assert(ForecastMap[nodeId].size() == FORECAST_WINDOW_SIZE);
        for (int i = 0; i < FORECAST_WINDOW_SIZE; i++)
        {
            it++;
            if (it == CurrentState::timeURIPartitionForecastsMap4Dim.end())
                break;

            auto predictedMaxForecasts = (it->second).find(uriPartition.second);
            if (predictedMaxForecasts == (it->second).end())
            {
                continue;
            }
            ForecastMap[nodeId][i].first.Add(predictedMaxForecasts->second);
            ForecastMap[nodeId][i].second = it->first;
        }
    }

    /*
    This method deletes a replica from the forecast map for a given node ID and URI partition.
    It retrieves the forecasted values for the specified URI partition from the current state and updates the forecast map accordingly.
    */
    void DeleteReplicaFromForecastMap(int nodeId, pair<string, string> uriPartition, CurrentState &currentState)
    {
        auto it = CurrentState::timeURIPartitionForecastsMap4Dim.find(currentState.time);
        if (it == CurrentState::timeURIPartitionForecastsMap4Dim.end())
        {
            return;
        }
        //assert(ForecastMap[nodeId].size() == FORECAST_WINDOW_SIZE);
        // When a URI partition fails to allocate to any node, ForecastMap is never initialized, log and skip.
        if (ForecastMap[nodeId].size() != FORECAST_WINDOW_SIZE) {
            cerr << "WARNING: ForecastMap[" << nodeId << "] size mismatch, skipping DeleteReplicaFromForecastMap." << endl;
            return;
        }

        for (int i = 0; i < FORECAST_WINDOW_SIZE; i++)
        {
            it++;
            if (it == CurrentState::timeURIPartitionForecastsMap4Dim.end())
                break;

            auto predictedMaxForecasts = (it->second).find(uriPartition.second);
            if (predictedMaxForecasts == (it->second).end())
            {
                break;
            }
            ForecastMap[nodeId][i].first.Delete(predictedMaxForecasts->second);
            ForecastMap[nodeId][i].second = it->first;
        }
    }

    /*
    This method finds the maximum forecast values for CPU, Memory, Storage, and Load from the forecast map for a given node ID.
    It iterates through the forecast map and updates the maximum forecast values accordingly.
    */
    void findMax(pair<ForecastValue,string> & pair, MaxForecast &maxForecastValues){
        if(pair.first.ForecastCPU>maxForecastValues.MaxForecastValue.ForecastCPU){
            maxForecastValues.MaxForecastValue.ForecastCPU=pair.first.ForecastCPU;
            maxForecastValues.CpuTime=pair.second;
        }
        if(pair.first.ForecastMemory>maxForecastValues.MaxForecastValue.ForecastMemory){
            maxForecastValues.MaxForecastValue.ForecastMemory=pair.first.ForecastMemory;
            maxForecastValues.MemoryTime=pair.second;
        }
        if(pair.first.ForecastStorage>maxForecastValues.MaxForecastValue.ForecastStorage){
            maxForecastValues.MaxForecastValue.ForecastStorage=pair.first.ForecastStorage;
            maxForecastValues.StorageTime=pair.second;
        }
        if(pair.first.ForecastLoad>maxForecastValues.MaxForecastValue.ForecastLoad){
            maxForecastValues.MaxForecastValue.ForecastLoad=pair.first.ForecastLoad;
            maxForecastValues.LoadTime=pair.second;
        }
    }

    /*
    This method retrieves the maximum forecast values for a given node ID.
    It checks if the forecast map for the node ID exists and if so, it iterates through the forecast values to find the maximum values.
    If the forecast map does not exist, it initializes a new forecast map for the node ID and adds the replicas to it.
    Finally, it returns the maximum forecast values.
    */
    MaxForecast maxNodeLoadPredictionV2(CurrentState &currentState, int nodeId)
    {
        if (ForecastMap.find(nodeId) != ForecastMap.end())
        {
            MaxForecast maxForecastValues = MaxForecast(currentState.time);
            for (auto &x : ForecastMap[nodeId])
            {
                findMax(x, maxForecastValues);
               
            }
            return maxForecastValues;
        }
        MaxForecast maxForecastValues = MaxForecast(currentState.time);
        ForecastMap[nodeId]=deque<pair<ForecastValue,string>>(FORECAST_WINDOW_SIZE);
        for (auto &uriParition : currentState.NodeIdToUriPartitionMap[nodeId])
        {
            AddReplicaToForecastMap(nodeId, uriParition, currentState);
        }

        for (auto &i : ForecastMap[nodeId])
        {
            findMax(i, maxForecastValues);
        }
         return maxForecastValues;
    }

    /*
    This method updates the forecast map for all nodes based on the current state of the system at every timestamp.
    It is called at every timestamp to ensure that the forecast map is up-to-date with the latest forecasted values.
    The method takes the current state as an argument and updates the forecast map for each node ID by deleting the oldest forecast value and adding a new one.
    */
    void UpdateForecastMap(CurrentState &currentState)
    {
        for(int nodeId=0;nodeId<NODE_COUNT;nodeId++){
            if(FORECAST_WINDOW_SIZE==0){
                break;
            }
            assert(ForecastMap[nodeId].size() == FORECAST_WINDOW_SIZE);
            ForecastMap[nodeId].pop_front();
            ForecastMap[nodeId].push_back({ForecastValue(), currentState.time});
            auto it = CurrentState::timeURIPartitionForecastsMap4Dim.find(currentState.time);
            if (it == CurrentState::timeURIPartitionForecastsMap4Dim.end())
            {
                return;
            }
            for (int i = 0; i < FORECAST_WINDOW_SIZE; i++)
            {
                it++;
                if(it == CurrentState::timeURIPartitionForecastsMap4Dim.end())
                    break;
                if(i==FORECAST_WINDOW_SIZE-1){
                    ForecastMap[nodeId][i].second = it->first;
                    for(auto &uriParition : currentState.NodeIdToUriPartitionMap[nodeId])
                    {
                        auto predictedMaxForecasts = (it->second).find(uriParition.second);
                        if (predictedMaxForecasts == (it->second).end())
                        {
                            break;
                        }
                        ForecastMap[nodeId][i].first.Add(predictedMaxForecasts->second);
                    }
                }
            }
        }
    }

    //* This method is created solely for debugging purposes.
    void Debug(){
        for(auto &x:ForecastMap){
            cout<<"NODE ID "<<x.first<<endl;
            for(auto &y:x.second){
                cout<<y.first.ForecastCPU<<" "<<y.first.ForecastMemory<<" "<<y.first.ForecastLoad<<" "<<" "<<y.first.ForecastStorage<<" "<<y.second<<",, ";
            }
        }
        cout<<endl;
    }
};
