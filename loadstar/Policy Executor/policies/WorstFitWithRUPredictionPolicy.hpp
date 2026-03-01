#pragma once
#include "../domain/CurrentState.hpp"
#include "../domain/IncomingRow.hpp"
#include "../common/common.h"
#include <assert.h>
#include "PolicyBase.hpp"
# include "../domain/Predictions.hpp"
class WorstFitWithRUPredictionPolicy : public PolicyBase
{
public:
    static int done;

    CurrentState Execute(CurrentState &currentState, IncomingRow &incomingRow, int migrationPolicyType,Predictions &predictions)
    {
        //  Separate new uriPartition and previously running uriPartition
        //  A method for this that will take the input as the currentState and incomingRow
        //  and it will return the pair of the new uriPartition and old uriPartition
        vector<pair<string, string>> existingUriPartition;
        vector<pair<string, string>> newUriPartition;
        for (auto it : incomingRow.uriPartition)
        {
            if (currentState.uriParitionPlacementMap.find(it) != currentState.uriParitionPlacementMap.end())
            {
                existingUriPartition.push_back(it);
            }
            else
            {
                newUriPartition.push_back(it);
            }
        }
        currentState.time = incomingRow.time;
        // cout<<"DONE IS "<<done<<endl;
        if(done==1){
            // cout<<"entered update forecast map from dene"<<endl;
            predictions.UpdateForecastMap(currentState);

        }
        done=1;
        // Update the current state using the existing uriPartition
        // A method for this that will take the input as the currentState and incomingRow and existingUriPartition
        // and will return the updated currentState and the uriPartition which I have to move to the other node
        cout<<"INTERMEDIATE STATE for TIME called "<<incomingRow.time<<endl;
        pair<CurrentState, vector<pair<string, string>>> a = IntermediateStateWithExistingUri(currentState, incomingRow, existingUriPartition, migrationPolicyType,predictions, true, true);
        cout<<"INTERMEDIATE STATE for TIME completed"<<incomingRow.time<<endl;
        CurrentState updatedState = a.first;
        vector<pair<string, string>> uriPartitionToMove = a.second;

        // update current state with uriPartitionToMove and newUriPartition
        // Allocate
        cout<<"ALLOCATION of new uriPartition for TIME "<<incomingRow.time<<endl;
        Allocation(updatedState, newUriPartition, incomingRow,predictions);
        cout<<"ALLOCATION of new uriPartition for TIME completed "<<incomingRow.time<<endl;
        cout<<"ALLOCATION of existing uriPartition for TIME "<<incomingRow.time<<endl;
        Allocation(updatedState, uriPartitionToMove, incomingRow, predictions,1);
        cout<<"ALLOCATION of existing uriPartition for TIME completed "<<incomingRow.time<<endl;
        return updatedState;
    }

private:
    float512 calculateScore(int nodeId, CurrentState &currentState,int512_t max_empty_space,pair<string,string> uriPartition, int512_t load, int512_t cpu, int512_t memory,int512_t storage, int softDisabled, int loopCounter, Predictions &predictions, MaxForecast &maxURIPartitionRUs){
        float512 score=0;
        MaxForecast maxNodeRUs= predictions.maxNodeLoadPredictionV2(currentState,nodeId);
        // MaxForecast maxNodeRUs= maxNodeLoadPredictionV2(currentState,nodeId);
        
        int512_t max_cpu = maxNodeRUs.MaxForecastValue.ForecastCPU;
        int512_t max_memory = maxNodeRUs.MaxForecastValue.ForecastMemory;
        int512_t max_storage = maxNodeRUs.MaxForecastValue.ForecastStorage;
        int512_t max_load = maxNodeRUs.MaxForecastValue.ForecastLoad;
        // cout<<"NODE ID "<<nodeId<<" MAX CPU "<<max_cpu<<" MAX MEMORY "<<max_memory<<" MAX STORAGE "<<max_storage<<" "<<"MAX LOAD "<<max_load<<endl;
        
        int512_t max_uri_cpu = maxURIPartitionRUs.MaxForecastValue.ForecastCPU;
        int512_t max_uri_memory = maxURIPartitionRUs.MaxForecastValue.ForecastMemory;
        int512_t max_uri_storage = maxURIPartitionRUs.MaxForecastValue.ForecastStorage;
        int512_t max_uri_load = maxURIPartitionRUs.MaxForecastValue.ForecastLoad;

        if(loopCounter==0 and (max_uri_load+max_load >NODE_LOAD or max_uri_cpu+max_cpu>CPU_LOAD or max_uri_memory+max_memory>MEMORY_LOAD or max_uri_storage+max_storage>STORAGE_LOAD)){
            // cout<<max_uri_load<<" "<<max_load<<" "<<max_uri_cpu<<" "<<max_cpu<<" "<<max_uri_memory<<" "<<max_memory<< " "<< max_uri_storage<< " "<<max_storage<<endl;
            // cout<<"NODE ID "<<nodeId<<" CANNOT ACCOMODATE DUE TO FUTURE PREDICTIONS "<<uriPartition.first<<" "<<uriPartition.second<<endl;
            // cout<<"YES"<<endl;
            return -1;
        }

        int512_t  empty_space=(softDisabled==1)?(NODE_LOAD-(loopCounter==0? maxNodeRUs.MaxForecastValue.ForecastLoad:currentState.NodeIdToNodeLoadMap[nodeId])):(NODE_LOAD_SOFT-(loopCounter==0? maxNodeRUs.MaxForecastValue.ForecastLoad:currentState.NodeIdToNodeLoadMap[nodeId]));
        // int512_t  empty_space=(softDisabled==1)?(NODE_LOAD-(loopCounter==0?maxNodeLoadPredictionV2(currentState,nodeId).MaxForecastValue.ForecastLoad:currentState.NodeIdToNodeLoadMap[nodeId])):(NODE_LOAD_SOFT-(loopCounter==0?maxNodeLoadPredictionV2(currentState,nodeId).MaxForecastValue.ForecastLoad:currentState.NodeIdToNodeLoadMap[nodeId]));
        int512_t  max_node_cpu_space = (softDisabled==1)?((loopCounter==0? maxNodeRUs.MaxForecastValue.ForecastCPU:currentState.NodeIdToNodeCPUMap[nodeId])):((loopCounter==0? maxNodeRUs.MaxForecastValue.ForecastCPU:currentState.NodeIdToNodeCPUMap[nodeId]));
        int512_t  max_node_memory_space = (softDisabled==1)?(loopCounter==0? maxNodeRUs.MaxForecastValue.ForecastMemory:currentState.NodeIdToNodeMemoryMap[nodeId]):((loopCounter==0? maxNodeRUs.MaxForecastValue.ForecastMemory:currentState.NodeIdToNodeMemoryMap[nodeId]));
        int512_t  max_node_storage_space = (softDisabled==1)?((loopCounter==0? maxNodeRUs.MaxForecastValue.ForecastStorage:currentState.NodeIdToNodeStorageMap[nodeId])):((loopCounter==0? maxNodeRUs.MaxForecastValue.ForecastStorage:currentState.NodeIdToNodeStorageMap[nodeId]));
        
        int512_t  cpu_space=currentState.NodeIdToNodeCPUMap[nodeId];
        int512_t  memory_space=currentState.NodeIdToNodeMemoryMap[nodeId];
        int512_t  storage_space=currentState.NodeIdToNodeStorageMap[nodeId];
        
        if(empty_space-load<0 or cpu_space+cpu>CPU_LOAD or memory_space+memory>MEMORY_LOAD or storage_space+storage>STORAGE_LOAD){
            // cout<<"NODE ID "<<nodeId<<" CANNOT ACCOMODATE USING CURRENT "<<uriPartition.first<<" "<<uriPartition.second<<endl;
            return -1;
        }

        float512 load_score=(((float512)empty_space-(float512)load)/((float512)max_empty_space))*(float512)LOAD_WEIGHT_FACTOR;
        float512 skewness_score=Skewness(nodeId,max_node_cpu_space,max_node_memory_space,max_node_storage_space,max_uri_cpu,max_uri_memory,max_uri_storage,currentState,uriPartition.second)*SKEWNESS_WEIGHT_FACTOR;
        
        float512 compatibilityScore =0;
        // for(auto &it: currentState.NodeIdToUriPartitionMap[nodeId]){
        //     int512_t score= compatibilityMatrix[currentState.uriPartitionClassMap[it.second]][currentState.uriPartitionClassMap[it.second]];
        //     score=score*currentState.uriParitionPlacementMap[it].Load;
        //     compatibilityScore+=float512(score);

        // }
        // if(currentState.NodeIdToUriPartitionMap[nodeId].size()==0){
        //     compatibilityScore=0;
        // }else{
        //     compatibilityScore=compatibilityScore/(float512)currentState.NodeIdToNodeLoadMap[nodeId];
        // }
        // compatibilityScore=compatibilityScore/(float512)currentState.NodeIdToNodeLoadMap[nodeId];
        
        // cout<<"NODE ID "<<nodeId<<" LOAD SCORE "<<load_score<<" SKEWNESS SCORE "<<skewness_score<<endl;
        score=load_score+skewness_score;
        // score=load_score+skewness_score;
        // cout<<"NODE ID "<<nodeId<<" SCORE "<<score<<endl;
        return score;
    }
    double Skewness(int nodeId,int512_t max_node_cpu_space, int512_t max_node_memory_space, int512_t max_node_storage_space, int512_t max_uri_cpu,int512_t max_uri_memory,int512_t max_uri_storage, CurrentState &currentState, string partition)
    {
        // cout<<"NODE ID "<<nodeId<<" CPU SPACE "<<cpu_space<<" MEMORY SPACE "<<memory_space<<" CPU "<<cpu<<" MEMORY "<<memory<<" PARTITION "<<partition<<endl;
        
        long double num = (long double)((float512)max_node_cpu_space+(float512)max_uri_cpu + ((float512)max_node_memory_space+(float512)max_uri_memory)+ ((float512)max_node_storage_space+(float512)max_uri_storage));
        long double denominator = sqrtl((long long)(((max_node_cpu_space+max_uri_cpu)*(max_node_cpu_space+max_uri_cpu))+ ((max_node_memory_space+max_uri_memory)*(max_node_memory_space+max_uri_memory))+ ((max_node_storage_space+max_uri_storage)*(max_node_storage_space+max_uri_storage)))*3);
        // cout<<"NODE ID "<<nodeId<<" DIFF "<<diff<<endl;
        // cout<<"NUM "<<num<<" DENOMINATOR "<<denominator<<endl;
        // cout<<num/denominator<<endl;
        // assert(denominator!=0);
        if (denominator==0){
            return 1;
        }
        double skew= acos(num/denominator);
        // cout<<"NODE ID "<<nodeId<<" SKEWNESS "<<skew<<endl;
        double pi=3.14159265359;
        // cout<<"NODE ID "<<nodeId<<" pi-slew "<<(pi-skew)/pi<<endl;

        // cout<<"NODE ID "<<nodeId<<" SKEWNESS "<<(piby4-abs(atan(double(1)) - diff))/piby4<<" for parition: "<<partition<<endl;
        return (pi-skew)/pi;

       
    }
void Allocation(CurrentState &currentState, vector<pair<string, string>> uriPartitionToAllocate, IncomingRow &incomingRow, Predictions &predictions,bool existing = false)
    {
        // take a set of pair of load and NodeId
        set<pair<boost::int512_t, int>> LoadNodeId;
        map<int, set<pair<string, string>>> NodeIdToPartitionUriMap;
        for (int i = 0; i < NODE_COUNT; i++)
        {
            for (auto it : currentState.NodeIdToUriPartitionMap[i])
            {
                NodeIdToPartitionUriMap[i].insert({it.second, it.first});
            }
        }

       for(auto &it: uriPartitionToAllocate){
         cout<<"STARTED ALLOCATION of "<<it.first<<" "<<it.second<<endl;
            bool FoundNodeUsingPred=false;
            int512_t candidateNode2=-1;
            for(int loop=0;loop<2;loop++){
                if(FORECAST_WINDOW_SIZE==0 and loop==0){
                    continue;
                }
                int candidateNode=-1;
                float512 goodness=-1;
                int512_t max_empty_space=1;
                for(int i=0;i<NODE_COUNT;i++){
                    int512_t empty_space=NODE_LOAD-(loop==0?predictions.maxNodeLoadPredictionV2(currentState,i).MaxForecastValue.ForecastLoad:currentState.NodeIdToNodeLoadMap[i]);
                    if(empty_space>max_empty_space){
                        max_empty_space=empty_space;
                    }
                }
                int512_t load,cpu,memory,storage;
                if(existing){
                    load = currentState.uriParitionPlacementMap[it].Load;
                    cpu= currentState.uriPartitionRuMap[it].Cpu;
                    memory= currentState.uriPartitionRuMap[it].Memory;
                    storage = currentState.uriPartitionRuMap[it].Storage;
                }
                else{
                    load = incomingRow.uriPartitionLoadReplicaMap[it].Load;
                    cpu= incomingRow.uriPartitionRuMap[it].Cpu;
                    memory= incomingRow.uriPartitionRuMap[it].Memory;
                    storage= incomingRow.uriPartitionRuMap[it].Storage;
                }
                int retry=0;
                if(NODE_LOAD_SOFT==NODE_LOAD)retry++;
                MaxForecast maxURIPartitionRUs=maxURIPartitionLoadPredictionV2(currentState,it);
                while(retry<2){
                        for(int i=0;i<NODE_COUNT;i++){
                            if(NodeIdToPartitionUriMap[i].find({it.second,it.first})!=NodeIdToPartitionUriMap[i].end()){
                                continue;
                            }
                            if(((NodeIdToPartitionUriMap[i].lower_bound({it.second,""}))!=NodeIdToPartitionUriMap[i].end()) and ((NodeIdToPartitionUriMap[i].lower_bound({it.second,""}))->first==it.second)){
                                continue;
                            }
                            //calculate goodness of each node
                            //if goodness is greater than the previous goodness then update the candidate node

                            float512 score=calculateScore(i,currentState,max_empty_space,it, load, cpu, memory,storage,retry,loop,predictions,maxURIPartitionRUs);
                            if(score>goodness){
                                goodness=score;
                                candidateNode=i;
                            }
                        }
                        
                        if(goodness==-1){
                            cout<<"RETRYING"<<endl;
                            retry++;
                        }
                        else{
                            break;
                        }
                }

                
                
                 if(goodness==-1 and loop ==0){
                        // cout<<"[DEBUG] NODE NOT FOUND USING PREDICTIONS"<<endl;
                        continue;
                 }
                 else if(goodness==-1 and loop ==1){
                    //  cout<<"[DEBUG] NODE NOT FOUND USING ACTUAL LOADS"<<endl;
                    // cout<<"[DEBUG] NODE NOT FOUND USING ACTUAL LOADS"<<endl;
                    // cout<<" CANNOT ACCOMODATE "<<it.first<<" "<<it.second<<endl;
                    // cout<<"URI PARTITION "<<it.first<<" "<<it.second<<" CANNOT ACCOMODATE "<<load<<" "<<cpu<<" "<<memory<<" "<<storage<<endl;
                     assert(1<0);
                     break;
                 }
                //  cout<<"NODE ID "<<candidateNode<<" IS THE CANDIDATE NODE"<<endl;
                    predictions.AddReplicaToForecastMap(candidateNode,it,currentState);
                    // if(candidateNode ==152){
                    // cout<<"INITIAL TOTAL LOAD "<<currentState.NodeIdToNodeLoadMap[candidateNode]<<endl;
                    // cout<< "URI LOAD "<<load<<endl;
                    // }
                    //update the current state maps
                    currentState.uriParitionPlacementMap[it].NodeId=candidateNode;
                    currentState.uriParitionPlacementMap[it].Load=load;
                    currentState.uriParitionPlacementMap[it].CPU=cpu;
                    currentState.uriParitionPlacementMap[it].Memory=memory;
                    currentState.uriParitionPlacementMap[it].Storage=storage;
                   
                    if(existing)
                        currentState.uriParitionPlacementMap[it].ReplicaId=currentState.uriParitionPlacementMap[it].ReplicaId;
                    else
                    currentState.uriParitionPlacementMap[it].ReplicaId=incomingRow.uriPartitionLoadReplicaMap[it].ReplicaId;
                    currentState.NodeIdToNodeLoadMap[candidateNode]+=load;
                    currentState.NodeIdToUriPartitionMap[candidateNode].insert({it.first,it.second});
                    NodeIdToPartitionUriMap[candidateNode].insert({it.second,it.first});
                    if(!existing)
                    currentState.uriPartitionRuMap[it]=incomingRow.uriPartitionRuMap[it];
                    currentState.NodeIdToNodeCPUMap[candidateNode]+=cpu;
                    currentState.NodeIdToNodeMemoryMap[candidateNode]+=memory;
                    currentState.NodeIdToNodeStorageMap[candidateNode]+=storage;

                    candidateNode2= candidateNode;
                    if(existing)
                        MigrationInfoMap[it].newNodeId=candidateNode;
                        break;
                    if(goodness!=-1){
                        if(loop==0){
                            // cout<<"[DEBUG] NODE FOUND USING PREDICTIONS"<<endl;
                            FoundNodeUsingPred=true;
                        }
                        break;
                    }
            }
        // cout<<"COMPLETED ALLOCATION of "<<it.first<<" "<<it.second<<" "<<"Node is"<< candidateNode2<< "using predictions"<<FoundNodeUsingPred<< endl;
        }
           //update the current state maps
        // sort the LoadNodeId
    }
    
};
