#pragma once
#include "../domain/CurrentState.hpp"
#include "../domain/IncomingRow.hpp"
#include "../domain/MigrationInfo.hpp"
#include "../common/common.h"
#include "../domain/ForecastValue.hpp"
#include "../domain/MaxForecast.hpp"
#include <string>
#include <map>
#include <vector>
#include <algorithm>
#include <fstream>
#include <assert.h>
#include <boost/multiprecision/cpp_int.hpp>
#include "../domain/Predictions.hpp"

using namespace std;
bool loadFlag = 0;
bool cpuFlag = 0;
bool memoryFlag = 0;
bool storageFlag = 0;
class PolicyBase
{

protected:
    boost::int512_t Hotness(CurrentState &currentState, int512_t overloadValue, pair<string, string> uriPartition, int migrationPolicyType, bool storageFlag, bool cpuFlag, bool memoryFlag, bool loadFlag)
    {

        Ru ru = currentState.uriPartitionRuMap[uriPartition];

        boost::int512_t load;
        if (storageFlag)
        {
            load = ru.Storage;
        }
        else if (cpuFlag)
        {
            load = ru.Cpu;
        }
        else if (memoryFlag)
        {
            load = ru.Memory;
        }
        else
        {
            load = currentState.uriParitionPlacementMap[uriPartition].Load;
        }
        if (migrationPolicyType <= 3) // heaviest lightest, binary
        {
            // cout<<"YESS"<<endl;
            return overloadValue;
        }
        return (boost::int512_t)ru.getRu() + load; // linear combination of Ru and Load
    }

    map<pair<string, string>, MigrationDetail> MigrationInfoMap;
    vector<pair<string, string>> MigrationUriPartitionCandidates(CurrentState &currentState, int nodeId, int migrationPolicyType, set<pair<string, string>> uriPartitionIncreasedLoad, bool isSmartFit, bool cpuFlag, bool memoryFlag, bool storageFlag, bool loadFlag, bool isV2Enabled = false, int512_t difference = 0, string overloadTime = "")
    {
        vector<pair<string, string>> uriPartitionToMove;
        auto cmp = [&](pair<boost::int512_t, pair<string, string>> a, pair<boost::int512_t, pair<string, string>> b)
        {
            // load,{uri,partition} -> a,b
            boost::int512_t HotnessA = Hotness(currentState, a.first, a.second, migrationPolicyType, storageFlag, cpuFlag, memoryFlag, loadFlag);
            boost::int512_t HotnessB = Hotness(currentState, b.first, b.second, migrationPolicyType, storageFlag, cpuFlag, memoryFlag, loadFlag);
            if (HotnessA == HotnessB)
            {
                // Can Confirm this have to check if the comparator works properly
                if (a.second.first == b.second.first)
                {
                    return a.second.second < b.second.second;
                }
                return a.second.first < b.second.first;
            }
            return HotnessA < HotnessB;
        };
        set<pair<boost::int512_t, pair<string, string>>, decltype(cmp)> LoadUriPartition(cmp); //{load,{ uri, partition}}
        vector<pair<string, string>> uriPartitionIncreasedLoadCandidates;
        // cout<<"PPPPPPPP"<<endl;

        for (auto it : currentState.NodeIdToUriPartitionMap[nodeId])
        {
            string uri = it.first;
            string partition = it.second;
            boost::int512_t load = currentState.uriParitionPlacementMap[{uri, partition}].Load;
            boost::int512_t cpu = currentState.uriPartitionRuMap[{uri, partition}].Cpu;
            boost::int512_t memory = currentState.uriPartitionRuMap[{uri, partition}].Memory;
            boost::int512_t storage = currentState.uriPartitionRuMap[{uri, partition}].Storage;

            if (uriPartitionIncreasedLoad.find({uri, partition}) != uriPartitionIncreasedLoad.end())
            {
                uriPartitionIncreasedLoadCandidates.push_back({uri, partition});
            }
            if (storageFlag)
            {
                if (isSmartFit and isV2Enabled)
                {
                    // cout<<"[DEBUG] V2 ENABLED STORAGE EXCEEDED"<<endl;
                    storage = CurrentState::timeURIPartitionForecastsMap4Dim[overloadTime][partition].ForecastStorage;
                }
                LoadUriPartition.insert({storage, {uri, partition}});
            }
            if (memoryFlag)
            {
                // cout<<"Memory exceeded"<<endl;
                if (isSmartFit and isV2Enabled)
                {
                    // cout<<"[DEBUG] V2 ENABLED MEMoRY EXCEEDED"<<endl;
                    memory = CurrentState::timeURIPartitionForecastsMap4Dim[overloadTime][partition].ForecastMemory;
                }
                // cout<<uri<<" "<<partition<<" "<<memory<<endl;
                LoadUriPartition.insert({memory, {uri, partition}});
            }
            if (cpuFlag)
            {
                if (isSmartFit and isV2Enabled)
                {
                    // cout<<"[DEBUG] V2 ENABLED CPU EXCEEDED"<<endl;
                    cpu = CurrentState::timeURIPartitionForecastsMap4Dim[overloadTime][partition].ForecastCPU;
                }
                LoadUriPartition.insert({cpu, {uri, partition}});
            }
            if (loadFlag)
            {
                if (isSmartFit and isV2Enabled)
                {
                    // cout<<"[DEBUG] V2 ENABLED LOAD EXCEEDED"<<endl;
                    load = CurrentState::timeURIPartitionForecastsMap4Dim[overloadTime][partition].ForecastLoad;
                }
                LoadUriPartition.insert({(load), {uri, partition}}); // modify TODo to handle which is to be inserted in the set
            }
        }

        boost::int512_t diff;
        if (isSmartFit)
        {
            if (isV2Enabled)
            {
                // cout<<"[DEBUG] V2 ENABLED"<<endl;
                diff = difference;
            }
            else
            {
                diff = (max(maxNodeLoadPrediction(currentState, nodeId) - NODE_LOAD, currentState.NodeIdToNodeLoadMap[nodeId] - NODE_LOAD));
            }
        }
        else
        {
            if (storageFlag)
            {
                diff = currentState.NodeIdToNodeStorageMap[nodeId] - STORAGE_LOAD;
            }
            if (memoryFlag)
            {
                diff = currentState.NodeIdToNodeMemoryMap[nodeId] - MEMORY_LOAD;
            }
            if (cpuFlag)
            {
                diff = currentState.NodeIdToNodeCPUMap[nodeId] - CPU_LOAD;
            }
            if (loadFlag)
            {
                diff = currentState.NodeIdToNodeLoadMap[nodeId] - NODE_LOAD;
            }
        }
        // cout<<"INTITAL DIFF IS "<<diff<<endl;
        while (diff > 0)
        {
            // cout<<"DIFF: "<<diff<<endl;
            // cout<<"eneterd again"<<endl;

            // cout << type;
            set<pair<boost::int512_t, pair<string, string>>>::iterator removeUriPartitionIt;
            //assert(LoadUriPartition.size() > 0);
            if (LoadUriPartition.size() == 0) {
                cerr << "WARNING: LoadUriPartition is empty, skipping migration candidates." << std::endl;
                break;
            }

            // binary
            if (migrationPolicyType == 1)
            {
                cout << "BINARY" << endl;
                removeUriPartitionIt = LoadUriPartition.lower_bound({diff, {"", ""}});
                // cout<<"URI PARTITION"<<endl;
                // for(auto it:LoadUriPartition){
                //     cout<<it.first<<" "<<it.second.first<<" "<<it.second.second<<endl;
                // }
                // cout<<"REMOVING"<<(*removeUriPartitionIt).second.first<<" "<<(*removeUriPartitionIt).second.second<<endl;
            }
            // lightest or hotness based on the RU values
            else if (migrationPolicyType == 2 || migrationPolicyType == 4)
                removeUriPartitionIt = LoadUriPartition.begin();
            // heaviest
            else if (migrationPolicyType == 3)
            {
                removeUriPartitionIt = LoadUriPartition.end();
                removeUriPartitionIt--;
            }
            else
            {
                cout << "Invalid Type" << endl;
                return uriPartitionToMove;
            }

            boost::int512_t load = 0, cpu = 0, memory = 0, storage = 0;
            string uri = "";
            string partition;
            if (removeUriPartitionIt == LoadUriPartition.end())
            {
                uri = (*LoadUriPartition.rbegin()).second.first;
                partition = (*LoadUriPartition.rbegin()).second.second;
                if (storageFlag)
                {
                    storage = (boost::int512_t)(*LoadUriPartition.rbegin()).first;
                }
                else
                {
                    storage = currentState.uriPartitionRuMap[{uri, partition}].Storage;
                }
                if (memoryFlag)
                {
                    memory = (boost::int512_t)(*LoadUriPartition.rbegin()).first;
                }
                else
                {
                    memory = currentState.uriPartitionRuMap[{uri, partition}].Memory;
                }
                if (cpuFlag)
                {
                    cpu = (boost::int512_t)(*LoadUriPartition.rbegin()).first;
                }
                else
                {
                    cpu = currentState.uriPartitionRuMap[{uri, partition}].Cpu;
                }
                if (loadFlag)
                {
                    load = (*LoadUriPartition.rbegin()).first;
                }
                else
                {
                    load = currentState.uriParitionPlacementMap[{uri, partition}].Load;
                }
            }
            else
            {
                uri = (*removeUriPartitionIt).second.first;
                partition = (*removeUriPartitionIt).second.second;
                if (storageFlag)
                {
                    storage = (boost::int512_t)(*removeUriPartitionIt).first;
                }
                else
                {
                    storage = currentState.uriPartitionRuMap[{uri, partition}].Storage;
                }
                if (memoryFlag)
                {
                    memory = (boost::int512_t)(*removeUriPartitionIt).first;
                }
                else
                {
                    memory = currentState.uriPartitionRuMap[{uri, partition}].Memory;
                }
                if (cpuFlag)
                {
                    cpu = (boost::int512_t)(*removeUriPartitionIt).first;
                }
                else
                {
                    cpu = currentState.uriPartitionRuMap[{uri, partition}].Cpu;
                }
                if (loadFlag)
                {
                    load = (*removeUriPartitionIt).first;
                }
                else
                {
                    load = currentState.uriParitionPlacementMap[{uri, partition}].Load;
                }
            }
            uriPartitionToMove.push_back({uri, partition});
            if (storageFlag)
            {
                diff -= storage;
                cout << "STORAGE" << storage << endl;
                LoadUriPartition.erase({(storage), {uri, partition}});
            }
            if (memoryFlag)
            {
                diff -= memory;
                cout << "MEMORY" << memory << endl;
                LoadUriPartition.erase({(memory), {uri, partition}});
            }
            if (cpuFlag)
            {
                diff -= cpu;
                cout << "CPU" << cpu << endl;
                LoadUriPartition.erase({(cpu), {uri, partition}});
            }
            if (loadFlag)
            {
                diff -= load;
                cout << "LOAD" << load << endl;
                LoadUriPartition.erase({(load), {uri, partition}});
            }
            //
            // DELETE THE URI FROM THE NO
            if (migrationPolicyType != 4)
            {
                // cout<<"DELETING"<<" "<<uri<<" "<<partition<<endl;
                currentState.uriParitionPlacementMap[{uri, partition}].NodeId = -1;
                currentState.NodeIdToNodeMemoryMap[nodeId] -= currentState.uriPartitionRuMap[{uri, partition}].Memory;
                currentState.NodeIdToNodeStorageMap[nodeId] -= currentState.uriPartitionRuMap[{uri, partition}].Storage;
                currentState.NodeIdToNodeCPUMap[nodeId] -= currentState.uriPartitionRuMap[{uri, partition}].Cpu;
                currentState.NodeIdToNodeLoadMap[nodeId] -= currentState.uriParitionPlacementMap[{uri, partition}].Load;
                currentState.NodeIdToUriPartitionMap[nodeId].erase({uri, partition}); // TODO CHECK an update apt tables and maps with updated ru values and laod ]
            }
        }
        cout << "COMPLETED MIGRATION for TIME " << currentState.time << endl;
        if (migrationPolicyType == 4)
        {
            bool IsCrossedThreshold = false;
            if (Hotness(currentState, 1, uriPartitionToMove.back(), 4, storageFlag, cpuFlag, memoryFlag, loadFlag) <= HOTNESS_THRESHOLD)
            {
                for (auto it : uriPartitionToMove)
                {
                    string uri = it.first;
                    string partition = it.second;
                    boost::int512_t load = currentState.uriParitionPlacementMap[{uri, partition}].Load;
                    currentState.uriParitionPlacementMap[{uri, partition}].NodeId = -1;
                    currentState.NodeIdToNodeLoadMap[nodeId] -= load;
                    currentState.uriParitionPlacementMap[{uri, partition}].Load = load;
                    currentState.NodeIdToUriPartitionMap[nodeId].erase({uri, partition});
                }
                return uriPartitionToMove;
            }
            else
            {
                IsCrossedThreshold = true;
                for (auto it : uriPartitionIncreasedLoadCandidates)
                {
                    string uri = it.first;
                    string partition = it.second;
                    boost::int512_t load = currentState.uriParitionPlacementMap[{uri, partition}].Load;
                    currentState.uriParitionPlacementMap[{uri, partition}].NodeId = -1;
                    currentState.NodeIdToNodeLoadMap[nodeId] -= load;
                    currentState.uriParitionPlacementMap[{uri, partition}].Load = load;
                    currentState.NodeIdToUriPartitionMap[nodeId].erase({uri, partition});
                }
                return uriPartitionIncreasedLoadCandidates;
            }
        }
        return uriPartitionToMove;
    }
    pair<CurrentState, vector<pair<string, string>>> IntermediateStateWithExistingUri(CurrentState &currentstate, IncomingRow &incomingRow, vector<pair<string, string>> existingUriPartition, int migrationPolicyType, Predictions &predictions, bool isSmartFit = false, bool isV2Enabled = false)
    {
        CurrentState updatedState = currentstate;
        updatedState.time = incomingRow.time;
        vector<pair<string, string>> UriPartitionToBeMigrated;
        set<pair<string, string>> uriPartitionSet;
        //* this will store all the replicas that have incresed load
        set<pair<string, string>> uriPartitionIncreasedLoad;
        // Create a set of existing URI partitions for efficient lookup
        for (auto it : existingUriPartition)
        {
            uriPartitionSet.insert(it);
        }
        vector<pair<string, string>> uriPartitionCompleted;

        // Identify URI partitions that are already completed
        for (auto &it : updatedState.uriParitionPlacementMap)
        {
            if (uriPartitionSet.find(it.first) == uriPartitionSet.end())
            {
                uriPartitionCompleted.push_back(it.first);
            }
        }

        // Remove completed URI partitions from nodes and maps
        cout << "REMOVING COMPLETED URI PARTITION for new TIME " << incomingRow.time << endl;
        for (auto it : uriPartitionCompleted)
        {
            string uri = it.first;
            string partition = it.second;
            int nodeId = currentstate.uriParitionPlacementMap[{uri, partition}].NodeId;
            // Adjust node load and remove URI partition mapping
            predictions.DeleteReplicaFromForecastMap(nodeId, it, updatedState);
            updatedState.DeleteReplica(nodeId, uri, partition);
        }
        cout << "REMOVING COMPLETED URI PARTITION for new TIME completed " << incomingRow.time << endl;

        // Process existing URI partitions
        cout << "UPDATING EXISTING URI PARTITION for new TIME " << incomingRow.time << endl;
        for (auto uriPartition : existingUriPartition)
        {
            NodeLoadReplica previousNodeLoadReplica = currentstate.uriParitionPlacementMap[uriPartition];
            LoadReplica incomingLoadReplica = incomingRow.uriPartitionLoadReplicaMap[uriPartition];
            Ru incomingRu = incomingRow.uriPartitionRuMap[uriPartition];
            Ru previousRu = currentstate.uriPartitionRuMap[uriPartition];
            // Ru incomingRu = incomingRow.uriPartitionRuMap[uriPartition];

            int nodeId = previousNodeLoadReplica.NodeId;
            if (updatedState.NodeIdToNodeLoadMap[nodeId] <= 0) {
                // Key inconsistency or rounding: node was zeroed under another key. Restore and proceed.
                updatedState.NodeIdToNodeLoadMap[nodeId] += previousNodeLoadReplica.Load;
                updatedState.NodeIdToNodeCPUMap[nodeId] += previousRu.Cpu;
                updatedState.NodeIdToNodeMemoryMap[nodeId] += previousRu.Memory;
                updatedState.NodeIdToNodeStorageMap[nodeId] += previousRu.Storage;
                updatedState.NodeIdToUriPartitionMap[nodeId].insert({uriPartition.first, uriPartition.second});
                updatedState.uriParitionPlacementMap[uriPartition] = previousNodeLoadReplica;
                updatedState.uriPartitionRuMap[uriPartition] = previousRu;
            }
            if (incomingLoadReplica.Load > previousNodeLoadReplica.Load)
            {
                uriPartitionIncreasedLoad.insert(uriPartition);
            }
            // Update the load of the URI partition
            updatedState.UpdateReplica(uriPartition, incomingLoadReplica, incomingRu, previousNodeLoadReplica, previousRu);
        }
        // Identify nodes with load exceeding the threshold and migrate partitions
        // if (!isSmartFit)
        //     for (int i = 0; i < NODE_COUNT; i++)
        //     {
        //         // cout<<"entered normal flow"<<endl;
        //         if (updatedState.NodeIdToNodeCPUMap[i] > CPU_LOAD)
        //         {
        //             bool cpuFlag = 1;
        //             vector<pair<string, string>> uriPartitionToMigrateFromCurrentNode = MigrationUriPartitionCandidates(updatedState, i, migrationPolicyType, uriPartitionIncreasedLoad, isSmartFit, cpuFlag, memoryFlag, storageFlag, loadFlag);
        //             cout << "Old NEED TO BE MIGRATED**UniqueReplicaId PartitionId NodeId Load ReplicaId" << endl;
        //             for (auto it : uriPartitionToMigrateFromCurrentNode)
        //             {
        //                 MigrationInfoMap[it].oldNodeId = i;
        //                 MigrationInfoMap[it].ReplicaId = updatedState.uriParitionPlacementMap[it].ReplicaId;
        //             }

        //             UriPartitionToBeMigrated.insert(UriPartitionToBeMigrated.end(), uriPartitionToMigrateFromCurrentNode.begin(), uriPartitionToMigrateFromCurrentNode.end());
        //         }
        //         if (updatedState.NodeIdToNodeMemoryMap[i] > MEMORY_LOAD)
        //         {
        //             bool memoryFlag = 1;
        //             vector<pair<string, string>> uriPartitionToMigrateFromCurrentNode = MigrationUriPartitionCandidates(updatedState, i, migrationPolicyType, uriPartitionIncreasedLoad, isSmartFit, cpuFlag, memoryFlag, storageFlag, loadFlag);
        //             cout << "Old NEED TO BE MIGRATED**UniqueReplicaId PartitionId NodeId Load ReplicaId" << endl;
        //             for (auto it : uriPartitionToMigrateFromCurrentNode)
        //             {
        //                 MigrationInfoMap[it].oldNodeId = i;
        //                 MigrationInfoMap[it].ReplicaId = updatedState.uriParitionPlacementMap[it].ReplicaId;
        //             }

        //             UriPartitionToBeMigrated.insert(UriPartitionToBeMigrated.end(), uriPartitionToMigrateFromCurrentNode.begin(), uriPartitionToMigrateFromCurrentNode.end());
        //         }
        //         if (updatedState.NodeIdToNodeStorageMap[i] > STORAGE_LOAD)
        //         {
        //             bool storageFlag = 1;
        //             vector<pair<string, string>> uriPartitionToMigrateFromCurrentNode = MigrationUriPartitionCandidates(updatedState, i, migrationPolicyType, uriPartitionIncreasedLoad, isSmartFit, cpuFlag, memoryFlag, storageFlag, loadFlag);
        //             cout << "Old NEED TO BE MIGRATED**UniqueReplicaId PartitionId NodeId Load ReplicaId" << endl;
        //             for (auto it : uriPartitionToMigrateFromCurrentNode)
        //             {
        //                 MigrationInfoMap[it].oldNodeId = i;
        //                 MigrationInfoMap[it].ReplicaId = updatedState.uriParitionPlacementMap[it].ReplicaId;
        //             }

        //             UriPartitionToBeMigrated.insert(UriPartitionToBeMigrated.end(), uriPartitionToMigrateFromCurrentNode.begin(), uriPartitionToMigrateFromCurrentNode.end());
        //         }

        //         if (updatedState.NodeIdToNodeLoadMap[i] > NODE_LOAD) // TODO add more checks and also take care cpu, memory, load, storage
        //         {
        //             cout << 'p';
        //             bool loadFlag = 1;
        //             vector<pair<string, string>> uriPartitionToMigrateFromCurrentNode = MigrationUriPartitionCandidates(updatedState, i, migrationPolicyType, uriPartitionIncreasedLoad, isSmartFit, cpuFlag, memoryFlag, storageFlag, loadFlag);
        //             // Print information about replicas that need to be migrated
        //             cout << "Old NEED TO BE MIGRATED**UniqueReplicaId PartitionId NodeId Load ReplicaId" << endl;
        //             for (auto it : uriPartitionToMigrateFromCurrentNode)
        //             {
        //                 MigrationInfoMap[it].oldNodeId = i;
        //                 MigrationInfoMap[it].ReplicaId = updatedState.uriParitionPlacementMap[it].ReplicaId;
        //             }

        //             UriPartitionToBeMigrated.insert(UriPartitionToBeMigrated.end(), uriPartitionToMigrateFromCurrentNode.begin(), uriPartitionToMigrateFromCurrentNode.end());
        //         }
        //     }
        if (isSmartFit == true)
        {
            if (isV2Enabled)
            {
                cout << "MIGRATING URI PARTITIONS at TIME " << incomingRow.time << endl;
                for (int i = 0; i < NODE_COUNT; i++)
                {
                    bool cpuFlag, memoryFlag, storageFlag, loadFlag;
                    cpuFlag = memoryFlag = storageFlag = loadFlag = 0;
                    if (updatedState.NodeIdToNodeCPUMap[i] > CPU_LOAD)
                    {
                        bool cpuFlag = 1;
                        cout << "CPU UPDATE STATE" << endl;
                        Migrate(UriPartitionToBeMigrated, updatedState, i, migrationPolicyType, uriPartitionIncreasedLoad, false, cpuFlag, memoryFlag, storageFlag, loadFlag);
                    }
                    if (updatedState.NodeIdToNodeMemoryMap[i] > MEMORY_LOAD)
                    {
                        bool memoryFlag = 1;
                        cout << "MEMORY UPDATE STATE" << endl;
                        Migrate(UriPartitionToBeMigrated, updatedState, i, migrationPolicyType, uriPartitionIncreasedLoad, false, cpuFlag, memoryFlag, storageFlag, loadFlag);
                    }
                    if (updatedState.NodeIdToNodeStorageMap[i] > STORAGE_LOAD)
                    {
                        bool storageFlag = 1;
                        cout << "STORAGE UPDATE STATE" << endl;
                        Migrate(UriPartitionToBeMigrated, updatedState, i, migrationPolicyType, uriPartitionIncreasedLoad, false, cpuFlag, memoryFlag, storageFlag, loadFlag);
                    }
                    if (updatedState.NodeIdToNodeLoadMap[i] > NODE_LOAD)
                    {
                        // cout<<i<<"NODE ID LOAD INCREASED"<<endl;
                        cout << 'p';
                        cout << "LOAD UPDATE STATE" << endl;
                        bool loadFlag = 1;
                        Migrate(UriPartitionToBeMigrated, updatedState, i, migrationPolicyType, uriPartitionIncreasedLoad, false, cpuFlag, memoryFlag, storageFlag, loadFlag);
                    }
                    MaxForecast maxForecastPredicitons = predictions.maxNodeLoadPredictionV2(updatedState, i);

                    if (maxForecastPredicitons.MaxForecastValue.ForecastCPU > CPU_LOAD)
                    {
                        bool cpuFlag = 1;
                        cout << "CPUUU UPDATE STATE" << endl;
                        Migrate(UriPartitionToBeMigrated, updatedState, i, migrationPolicyType, uriPartitionIncreasedLoad, isSmartFit, cpuFlag, memoryFlag, storageFlag, loadFlag, true, maxForecastPredicitons.MaxForecastValue.ForecastCPU - CPU_LOAD, maxForecastPredicitons.CpuTime);
                    }
                    maxForecastPredicitons = predictions.maxNodeLoadPredictionV2(updatedState, i);
                    if (maxForecastPredicitons.MaxForecastValue.ForecastMemory > MEMORY_LOAD)
                    {
                        bool memoryFlag = 1;
                        cout << "MEMORYyy UPDATE STATE" << endl;
                        Migrate(UriPartitionToBeMigrated, updatedState, i, migrationPolicyType, uriPartitionIncreasedLoad, isSmartFit, cpuFlag, memoryFlag, storageFlag, loadFlag, true, maxForecastPredicitons.MaxForecastValue.ForecastMemory - MEMORY_LOAD, maxForecastPredicitons.MemoryTime);
                    }
                    maxForecastPredicitons = predictions.maxNodeLoadPredictionV2(updatedState, i);
                    if (maxForecastPredicitons.MaxForecastValue.ForecastStorage > STORAGE_LOAD)
                    {
                        bool storageFlag = 1;
                        cout << "STORAGEEEEE UPDATE STATE" << endl;
                        Migrate(UriPartitionToBeMigrated, updatedState, i, migrationPolicyType, uriPartitionIncreasedLoad, isSmartFit, cpuFlag, memoryFlag, storageFlag, loadFlag, true, maxForecastPredicitons.MaxForecastValue.ForecastStorage - STORAGE_LOAD, maxForecastPredicitons.StorageTime);
                    }

                    maxForecastPredicitons = predictions.maxNodeLoadPredictionV2(updatedState, i);
                    if (maxForecastPredicitons.MaxForecastValue.ForecastLoad > NODE_LOAD)
                    {
                        // cout<<i<<"NODE ID LOAD INCREASED"<<endl;
                        cout << 'p';
                        bool loadFlag = 1;
                        cout << "LOADDDDD UPDATE STATE" << endl;
                        Migrate(UriPartitionToBeMigrated, updatedState, i, migrationPolicyType, uriPartitionIncreasedLoad, isSmartFit, cpuFlag, memoryFlag, storageFlag, loadFlag, true, maxForecastPredicitons.MaxForecastValue.ForecastLoad - NODE_LOAD, maxForecastPredicitons.LoadTime);
                    }
                }
                cout << "FINAL MIGRATING URI PARTITIONS at TIME " << incomingRow.time << endl;
            }
            else
                for (int i = 0; i < NODE_COUNT; i++)
                {
                    int512_t PredLoad = maxNodeLoadPrediction(updatedState, i);
                    MaxForecast maxForecastPredicitons = predictions.maxNodeLoadPredictionV2(updatedState, i);
                    // cout<<PredLoad<<endl;
                    if (PredLoad > NODE_LOAD or updatedState.NodeIdToNodeLoadMap[i] > NODE_LOAD)
                    {
                        // cout<<i<<"NODE ID LOAD INCREASED"<<endl;
                        cout << 'p';
                        vector<pair<string, string>> uriPartitionToMigrateFromCurrentNode = MigrationUriPartitionCandidates(updatedState, i, migrationPolicyType, uriPartitionIncreasedLoad, isSmartFit, cpuFlag, memoryFlag, storageFlag, loadFlag);
                        // Print information about replicas that need to be migrated
                        cout << "Old NEED TO BE MIGRATED**UniqueReplicaId PartitionId NodeId Load ReplicaId" << endl;
                        for (auto it : uriPartitionToMigrateFromCurrentNode)
                        {
                            predictions.DeleteReplicaFromForecastMap(i, it, updatedState);
                            //    cout<<"MIGRATING REPLICA" <<it.first<<" "<<it.second <<endl;
                            //    cout<<"PREDICTIONS"<<endl;
                            //    predictions.Debug();
                        }

                        for (auto it : uriPartitionToMigrateFromCurrentNode)
                        {
                            MigrationInfoMap[it].oldNodeId = i;
                            MigrationInfoMap[it].ReplicaId = updatedState.uriParitionPlacementMap[it].ReplicaId;
                        }

                        UriPartitionToBeMigrated.insert(UriPartitionToBeMigrated.end(), uriPartitionToMigrateFromCurrentNode.begin(), uriPartitionToMigrateFromCurrentNode.end());
                    }
                }
        }
        // Print information about replicas that need to be migrated
        return {updatedState, UriPartitionToBeMigrated};
    }

public:
    // TODO
    //  boost::int512_t  caclulateload(CurrentState, map,int FORECAST_WINDOW_SIZE,int node_id){
    //      //
    //  }
    boost::int512_t maxNodeLoadPrediction(CurrentState &currentState, int nodeId)
    {
        // cout<<"THE NODE ID IS::"<<nodeId<<endl;
        vector<boost::int512_t> sumOfPredictedLoads(FORECAST_WINDOW_SIZE, 0); //
        for (auto &uriParition : currentState.NodeIdToUriPartitionMap[nodeId])
        {
            auto it = CurrentState::timeURIPartitionForecastsMap.find(currentState.time);
            // cout<<"max node predictions at time:"<<currentState.time<<endl;
            if (it == CurrentState::timeURIPartitionForecastsMap.end())
            {
                continue;
            }
            for (int i = 0; i < FORECAST_WINDOW_SIZE; i++)
            {
                it++;
                if (it == CurrentState::timeURIPartitionForecastsMap.end())
                    break;

                auto predictedLoadIt = (it->second).find(uriParition);
                if (predictedLoadIt == (it->second).end())
                {
                    break;
                }
                // cout<<it->first<<" time "<<(it->second).find(uriParition)->first.first<<"uri "<<(it->second).find(uriParition)->first.second<<" partition "<<(it->second).find(uriParition)->second<<" load"<<endl;

                sumOfPredictedLoads[i] += predictedLoadIt->second;
            }
        }
        boost::int512_t a = 0;
        for (auto &i : sumOfPredictedLoads)
        {
            if (i > a)
                a = i;
        }
        return a;
    }
    MaxForecast maxNodeLoadPredictionV2(CurrentState &currentState, int nodeId)
    {
        // cout<<"THE NODE ID IS::"<<nodeId<<endl;
        MaxForecast maxForecastValues = MaxForecast(currentState.time);
        vector<pair<ForecastValue, string>> sumOfPredictedLoads(FORECAST_WINDOW_SIZE); // forecast value and time
        for (auto &uriParition : currentState.NodeIdToUriPartitionMap[nodeId])
        {
            auto it = CurrentState::timeURIPartitionForecastsMap4Dim.find(currentState.time);
            // cout<<"max node predictions at time:"<<currentState.time<<endl;
            if (it == CurrentState::timeURIPartitionForecastsMap4Dim.end())
            {
                continue;
            }
            for (int i = 0; i < FORECAST_WINDOW_SIZE; i++)
            {
                sumOfPredictedLoads[i].second = it->first;

                it++;
                if (it == CurrentState::timeURIPartitionForecastsMap4Dim.end())
                    break;

                auto predictedMaxForecasts = (it->second).find(uriParition.second);
                if (predictedMaxForecasts == (it->second).end())
                {
                    break;
                }
                // cout<<it->first<<" time "<<(it->second).find(uriParition)->first.first<<"uri "<<(it->second).find(uriParition)->first.second<<" partition "<<(it->second).find(uriParition)->second<<" load"<<endl;
                sumOfPredictedLoads[i].first.Add(predictedMaxForecasts->second);
            }
        }

        for (auto &i : sumOfPredictedLoads)
        {
            if (i.first.ForecastCPU > maxForecastValues.MaxForecastValue.ForecastCPU)
            {
                maxForecastValues.MaxForecastValue.ForecastCPU = i.first.ForecastCPU;
                maxForecastValues.CpuTime = i.second;
            }
            if (i.first.ForecastMemory > maxForecastValues.MaxForecastValue.ForecastMemory)
            {
                maxForecastValues.MaxForecastValue.ForecastMemory = i.first.ForecastMemory;
                maxForecastValues.MemoryTime = i.second;
            }
            if (i.first.ForecastStorage > maxForecastValues.MaxForecastValue.ForecastStorage)
            {
                maxForecastValues.MaxForecastValue.ForecastStorage = i.first.ForecastStorage;
                maxForecastValues.StorageTime = i.second;
            }
            if (i.first.ForecastLoad > maxForecastValues.MaxForecastValue.ForecastLoad)
            {
                maxForecastValues.MaxForecastValue.ForecastLoad = i.first.ForecastLoad;
                maxForecastValues.LoadTime = i.second;
            }
        }
        return maxForecastValues;
    }

    MaxForecast maxURIPartitionLoadPredictionV2(CurrentState &currentState, pair<string, string> uriPartition)
    {
        // cout<<"THE NODE ID IS::"<<nodeId<<endl;
        MaxForecast maxForecastValues = MaxForecast(currentState.time);
        vector<pair<ForecastValue, string>> sumOfPredictedLoads(FORECAST_WINDOW_SIZE); // forecast value and time
        auto it = CurrentState::timeURIPartitionForecastsMap4Dim.find(currentState.time);
        for (int i = 0; i < FORECAST_WINDOW_SIZE; i++)
        {
            sumOfPredictedLoads[i].second = currentState.time;

            it++;
            if (it == CurrentState::timeURIPartitionForecastsMap4Dim.end())
                break;

            auto predictedMaxForecasts = (it->second).find(uriPartition.second);
            if (predictedMaxForecasts == (it->second).end())
            {
                break;
            }
            // cout<<it->first<<" time "<<(it->second).find(uriParition)->first.first<<"uri "<<(it->second).find(uriParition)->first.second<<" partition "<<(it->second).find(uriParition)->second<<" load"<<endl;
            sumOfPredictedLoads[i].first.Add(predictedMaxForecasts->second);
        }

        for (auto &i : sumOfPredictedLoads)
        {
            if (i.first.ForecastCPU > maxForecastValues.MaxForecastValue.ForecastCPU)
            {
                maxForecastValues.MaxForecastValue.ForecastCPU = i.first.ForecastCPU;
                maxForecastValues.CpuTime = i.second;
            }
            if (i.first.ForecastMemory > maxForecastValues.MaxForecastValue.ForecastMemory)
            {
                maxForecastValues.MaxForecastValue.ForecastMemory = i.first.ForecastMemory;
                maxForecastValues.MemoryTime = i.second;
            }
            if (i.first.ForecastStorage > maxForecastValues.MaxForecastValue.ForecastStorage)
            {
                maxForecastValues.MaxForecastValue.ForecastStorage = i.first.ForecastStorage;
                maxForecastValues.StorageTime = i.second;
            }
            if (i.first.ForecastLoad > maxForecastValues.MaxForecastValue.ForecastLoad)
            {
                maxForecastValues.MaxForecastValue.ForecastLoad = i.first.ForecastLoad;
                maxForecastValues.LoadTime = i.second;
            }
        }

        return maxForecastValues;
    }
    void PrintMigrationInfo(string time, char *fileprefix)
    {
        std::ofstream myfile;
        char migrationFile[500];
        strcpy(migrationFile, fileprefix);
        strcat(migrationFile, "_MigrationInfo.csv");
        myfile.open(migrationFile, std::ios_base::app);
        for (auto it : MigrationInfoMap)
        {
            // Add Time,UniqueReplicaId,PartitionId,OldNodeId,NewNodeId,ReplicaId;
            myfile << time << "," << it.first.first << "," << it.first.second << "," << it.second.oldNodeId << "," << it.second.newNodeId << "," << it.second.ReplicaId << endl;
        }
    }
    void Migrate(vector<pair<string, string>> &uriPartionToBeMigrated, CurrentState &currentState, int nodeId, int migrationPolicyType, set<pair<string, string>> &uriPartitionIncreasedLoad, bool isSmartFit, bool cpuFlag, bool memoryFlag, bool storageFlag, bool loadFlag, bool isV2Enabled = false, boost::int512_t difference = 0, string time = "")
    {
        vector<pair<string, string>> uriPartitionToMigrateFromCurrentNode;
        bool isForecasted = 0;
        if (isSmartFit == false)
        {
            uriPartitionToMigrateFromCurrentNode = MigrationUriPartitionCandidates(currentState,
                                                                                   nodeId, migrationPolicyType, uriPartitionIncreasedLoad, isSmartFit, cpuFlag, memoryFlag, storageFlag, loadFlag);
        }
        else
        {
            uriPartitionToMigrateFromCurrentNode = MigrationUriPartitionCandidates(currentState,
                                                                                   nodeId, migrationPolicyType, uriPartitionIncreasedLoad, isSmartFit, cpuFlag, memoryFlag, storageFlag, loadFlag, isV2Enabled, difference, time);
            isForecasted = 1;
        }
        for (auto it : uriPartitionToMigrateFromCurrentNode)
        {
            MigrationInfoMap[it].oldNodeId = nodeId;
            MigrationInfoMap[it].newNodeId = currentState.uriParitionPlacementMap[it].NodeId;
            MigrationInfoMap[it].ReplicaId = currentState.uriParitionPlacementMap[it].ReplicaId;
            MigrationInfoMap[it].Flag = isForecasted;
            currentState.DeleteReplica(nodeId, it.first, it.second);
        }
        uriPartionToBeMigrated.insert(uriPartionToBeMigrated.end(), uriPartitionToMigrateFromCurrentNode.begin(), uriPartitionToMigrateFromCurrentNode.end());
    }
};