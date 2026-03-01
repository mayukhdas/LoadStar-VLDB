#pragma once
#include <assert.h>
#include "../domain/CurrentState.hpp"
#include "../domain/IncomingRow.hpp"
#include "../common/common.h"
// #include "../policies/BestFirstPolicy.hpp"
// #include "../policies/WorstFirstPolicy.hpp"
// #include "../policies/BestFirstDecreasingPolicy.hpp"
// #include "../policies/FirstFitPolicy.hpp"
// #include "../policies/LocalAutoScaling.hpp"
// #include "../policies/WorstFitWithPredictionPolicy.hpp" 
// #include "../policies/BestFitWithPredictionPolicy.hpp" 
// #include "../policies/WorstFirstWithRUs.hpp" 
#include "../policies/WorstFitWithRUPredictionPolicy.hpp" 
#include <boost/multiprecision/cpp_int.hpp>
#include "../domain/Predictions.hpp"

namespace boost
{
    using int512_t = boost::multiprecision::int512_t;
}
using namespace std;
class Policy
{
private:
public:
    void printAllocation(vector<pair<string, string>> uriPartitionToAllocate, CurrentState &currentState, IncomingRow &incomingRow)
    {
        // cout << "URI PARTITION NODE LOAD REPLICAID" << endl;
        for (auto it : uriPartitionToAllocate)
        {
            string uri = it.first;
            string partition = it.second;
        }
    }

public:
    CurrentState Execute(CurrentState &currentState, IncomingRow incomingRow, int policyType, int migrationPolicyType, char * migrationFile, int K,Predictions &predictions)
    {
        WorstFitWithRUPredictionPolicy worstFitWithRUPredictionPolicy;

        switch (policyType)
        {
        // case 1:
        // {
        //     BestFirstPolicy bestFirstPolicy;
        //     CurrentState newState = bestFirstPolicy.Execute(currentState, incomingRow, migrationPolicyType,predictions);
        //     bestFirstPolicy.PrintMigrationInfo(incomingRow.time,migrationFile);
        //     return newState;
        // }
        // case 2:
        // {
        //     WorstFirstPolicy worstFirstPolicy;
        //     CurrentState newState = worstFirstPolicy.Execute(currentState, incomingRow, migrationPolicyType,predictions);
        //     worstFirstPolicy.PrintMigrationInfo(incomingRow.time,migrationFile);
        //     return newState;
        // }
        // case 3:
        // {
        //     BestFirstDecreasingPolicy bestFirstDecreasingPolicy;
        //     CurrentState newState = bestFirstDecreasingPolicy.Execute(currentState, incomingRow, migrationPolicyType,predictions);
        //     bestFirstDecreasingPolicy.PrintMigrationInfo(incomingRow.time,migrationFile);
        //     return newState;
        // }
        // case 4:
        // {
        //     FirstFitPolicy firstFitPolicy;
        //     CurrentState newState = firstFitPolicy.Execute(currentState, incomingRow, migrationPolicyType,predictions);
        //     firstFitPolicy.PrintMigrationInfo(incomingRow.time,migrationFile);
        //     return newState;
        // }
        // case 5:
        // {
        //     LocalAutoScaling localAutoScaling;
        //     CurrentState newState = localAutoScaling.Execute(currentState, incomingRow, migrationPolicyType,predictions);
        //     localAutoScaling.PrintMigrationInfo(incomingRow.time,migrationFile);
        //     return newState;
        // }
        // case 6:{
        //     WorstFitWithPredictionPolicy worstFitWithPredictionPolicy;
        //     CurrentState newState = worstFitWithPredictionPolicy.Execute(currentState, incomingRow, migrationPolicyType,K,predictions);
        //     worstFitWithPredictionPolicy.PrintMigrationInfo(incomingRow.time,migrationFile);
        //     return newState;
        // }
        // case 7:{
        //     BestFitWithPredictionPolicy bestFitWithPredictionPolicy;
        //     CurrentState newState = bestFitWithPredictionPolicy.Execute(currentState, incomingRow, migrationPolicyType,K,predictions);
        //     bestFitWithPredictionPolicy.PrintMigrationInfo(incomingRow.time,migrationFile);
        //     return newState;
        // }
        // case 8:{
        //     WorstFirstWithRUs worstFitWithRUs;
        //     CurrentState newState = worstFitWithRUs.Execute(currentState, incomingRow, migrationPolicyType,predictions);
        //     worstFitWithRUs.PrintMigrationInfo(incomingRow.time,migrationFile);
        //     return newState;
        // }
        case 9:{
            cout<<"TIME IS :"<<incomingRow.time<<endl;
            CurrentState newState = worstFitWithRUPredictionPolicy.Execute(currentState, incomingRow, migrationPolicyType,predictions);
            worstFitWithRUPredictionPolicy.PrintMigrationInfo(incomingRow.time,migrationFile);
            return newState;
        }
        default:
            return currentState;
            break;
        }

    }
};
