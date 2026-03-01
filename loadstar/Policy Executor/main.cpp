#include <bits/stdc++.h>
#include "common/common.h"
#include "domain/CurrentState.hpp"
#include "domain/IncomingRow.hpp"
#include "domain/GlobalState.hpp"
#include "fileReader/FileReader.hpp"
#include "policyExecutor/Policy.hpp"
#include <boost/multiprecision/cpp_int.hpp>
// #include "policies/OfflineVRO.hpp"
#include "domain/Predictions.hpp"
#include "policies/WorstFitWithRUPredictionPolicy.hpp"
using namespace std;
namespace boost
{
    using int512_t = boost::multiprecision::int512_t;
}

//defining the variables (declared in common.h)
boost::int512_t NODE_LOAD;
boost::int512_t NODE_LOAD_SOFT;
boost::int512_t HOTNESS_THRESHOLD;
double LOAD_WEIGHT_FACTOR=1;
double SKEWNESS_WEIGHT_FACTOR;
boost::int512_t CPU_LOAD;
boost::int512_t MEMORY_LOAD;
boost::int512_t STORAGE_LOAD;
int FORECAST_WINDOW_SIZE;
int NODE_COUNT;

//variable to update predictions after first timestamp
int WorstFitWithRUPredictionPolicy::done; 

//definition of variables (used for eigen paper implementation)
double ALPHA = 1;
double GAMMA = 1;
double THETA_MIN = 0; // angle in radians
double THETA_MAX = 1.57; // pi/2
float512 Q = 5000000; // cost factor 
int It = 20; // number of iterations
int parity = 0; // to check if the current timestamp is a multiple of 36 (to run the offline VRO)
int iter=0;

//defining maps to store the forecasts 
//timeURIPartitionForecastsMap is used for the load forecasts only
//timeURIPartitionForecastsMap4Dim is used for the d-forecasts
map<string, map<pair<string,string>,boost::int512_t>> CurrentState::timeURIPartitionForecastsMap;
map<string, map<string,ForecastValue>> CurrentState::timeURIPartitionForecastsMap4Dim;
map<string,int> CurrentState::uriPartitionClassMap;

//defining compatibility matrix different types of partitions (optional for now)
// vector<vector<int>> compatibilityMatrix={{1,0,0,0},{0,1,0,1},{0,0,1,0},{1,0,0,1}};

//Main program
int main(int argc, char *argv[])
{
    Policy policy;
    GlobalState globalState;
    std::ofstream myfile, myfile2;

    // Reading a File and checking arguments for number of parameters
    // if 6, it is used for reading CPU and memory for simple policies like best fit
    // if 5, it is used to read forecasts only for load resource
    // if 10, it reads forecast files across all 4 dimensions of load, cpu, memory and storage
    FileReader fileReader;
    bool optionalArgsForecastFile=atoi(argv[2])==6;
    bool isWorstFitWithPredictionsAvailable=atoi(argv[2])==5;
    bool isWorstFitWithCPUMemory=atoi(argv[2])==10;
    
    fstream uriPartitionClasses;
    
    //Reading the forecast files for each of the resources
    fstream cpuForecastfile, loadForecastfile, memoryForecastfile, storageForecastfile;
    if(isWorstFitWithCPUMemory ==1){
        cpuForecastfile.open(argv[3]);
        loadForecastfile.open(argv[4]);
        memoryForecastfile.open(argv[5]);
        storageForecastfile.open(argv[6]);
        if (cpuForecastfile.is_open())
        {
            CurrentState::generate_uri_partition_ForecastRU_map(cpuForecastfile, "cpu");
            cpuForecastfile.close();
        }
        if (memoryForecastfile.is_open())
        {
            CurrentState::generate_uri_partition_ForecastRU_map(memoryForecastfile, "memory");
            memoryForecastfile.close();
        }
        if (loadForecastfile.is_open())
        {
            CurrentState::generate_uri_partition_ForecastRU_map(loadForecastfile, "load");
            loadForecastfile.close();
        }
        if (storageForecastfile.is_open()){
            CurrentState::generate_uri_partition_ForecastRU_map(storageForecastfile, "storage");
            storageForecastfile.close();
        }
    }

    // Optional: reading the file with partition to class mapping 
    // if (uriPartitionClasses.is_open())
    // {   cout<<"OPENED"<<endl;
    //         CurrentState::generate_uri_partition_class_map(uriPartitionClasses);
    // }

    CurrentState currentState("");

    // Taking node parameters from the input config file
    string prefix="";
    cout << "Enter the Skewness parameter: ";
    cin >> SKEWNESS_WEIGHT_FACTOR;
    prefix+="skewness-";
    prefix+=to_string((long long)(10*SKEWNESS_WEIGHT_FACTOR));
    cout << "Enter the Load Capacity of the Node: ";
    cin >> NODE_LOAD;
    NODE_LOAD_SOFT = NODE_LOAD;
    prefix+="nodes-";
    cout << "Enter the Number of Nodes: ";
    cin >> NODE_COUNT;
    prefix+=to_string(NODE_COUNT);
    cout << "Enter the Hard Load Capacity of the Node: ";
    cin >> NODE_LOAD;
    prefix+=" load-";
    prefix+=to_string((long long )NODE_LOAD);
    cout << "Enter the CPU Thresold of the Node: ";
    cin >> CPU_LOAD;
    prefix+=" CPU-";
    prefix+=to_string((long long )CPU_LOAD);
    cout << "Enter the Memory Thresold of the Node: ";
    cin >> MEMORY_LOAD;
    prefix+=" MEMORY-";
    prefix+=to_string((long long)MEMORY_LOAD);
    cout << "Enter the Storage Thresold of the Node: ";
    cin >> STORAGE_LOAD;
    prefix+=" STORAGE-";
    prefix+=to_string((long long)MEMORY_LOAD);
    bool SoftLoad = false;
    cout << "Do you want to enter the Soft Load Capacity of the Node? (Y/N): ";
    char ch;
    cin >> ch;
    if (ch == 'Y' or ch == 'y'){
        SoftLoad = true;
    }

    //Optional soft load threshold for RU
    if (SoftLoad)
    {
        cout << "\nEnter the Soft Load Capacity of the Node: ";
        cin >> NODE_LOAD_SOFT;
        prefix+="soft-y";
        prefix+=to_string((long long)NODE_LOAD_SOFT);
    }

    // Taking Policy from user
    cout << "What policy you want to use: \n1. BestFirst\n2. WorstFit\n3. BestFirstDecreasing\n4. FirstFit\n5. LocalAutoScaling\n6. WorstFitWithForecasts\n7. BestFitWithForecasts\n8. WorstFitWithCPUMemory\n9. WorstFitWithCPUMemoryRUPrediction\n";
    int policyType;
    cin >> policyType;
    int migrationPolicyType = 4; // online vro
    if(policyType==1){
        prefix+="best-f-";
    }
    else if(policyType==2){
        prefix+="worst-f-";
    }
    else if(policyType==3){
        prefix+="best-first-d-";
    }
    else if(policyType==4){
        prefix+="first-f-";
    }
    else if(policyType==5){
        prefix+="local-auto-";
    }
    else if(policyType==6){
        prefix+="smart-worst-fit-";
    }
    else if(policyType==7){
        prefix+="smart-best-fit-";
    }
    else{
        prefix+="worst-fit-RUs-";
    }
    
    if (policyType == 5)
    {
        cout << "Enter the Hotness Threshold: ";
        cin >> HOTNESS_THRESHOLD;
        prefix+="h-thres";
        prefix+=to_string((long long)HOTNESS_THRESHOLD);
    }

    else
    {
    if (policyType == 6 or policyType ==9)
    {
        cout << "Enter the Window Size of Prediction (FORECAST_WINDOW_SIZE): ";
        cin >> FORECAST_WINDOW_SIZE;
        prefix+=to_string((long long)FORECAST_WINDOW_SIZE);
    }
        cout << "What Migration Policy you want to use: \n1. Binary\n2. Lightes\n3. Heaviest\n";
        cin >> migrationPolicyType;
        prefix+="mig-";
        if(migrationPolicyType==1){
            prefix+="bin";
        }
        else if(migrationPolicyType==2){
            prefix+="lig";
        }
        else{
            prefix+="heav";
        }
        prefix+="- ";
    }
    int i=0;
    while(argv[1][i]!='\0'){
        prefix+=argv[1][i++];
    }

    // Taking Migration technique as input
    // OfflineVro offlineVro;
    myfile.open("MigrationInfo.csv", std::ios_base::out);
    myfile << "Time,UniqueReplicaId,PartitionId,OldNodeId,NewNodeId,ReplicaId" << endl;
    
    //defines the output file name in accordance to your choice of parameters in input config file
    int n = prefix.length();
    char arr[n + 1];
    auto first = prefix.begin();
    auto last = prefix.end();
    copy(first, last, arr);
    arr[n] = '\0';

    Predictions predictions(FORECAST_WINDOW_SIZE);

    std::ofstream myfile1;
    char NodeClearFile[500];
    strcpy(NodeClearFile,arr);
    strcat(NodeClearFile,"_NodeState.csv");
    myfile1.open(NodeClearFile, std::ios_base::out);
    myfile1 << "Time,NodeId,CurrLoad,CurrCpu,CurrMemory,CurrStorage" << endl;
    myfile1.close();
    const int PROGRESS_INTERVAL = 500;  // print progress to stderr every N timestamps
    while(1){

        IncomingRow incomingRow=fileReader.read_next_timestamp(argv[1],optionalArgsForecastFile,isWorstFitWithPredictionsAvailable,isWorstFitWithCPUMemory);
        if(incomingRow.time=="END OF FILE"){
            cout<<"END OF FILE"<<endl;
            cerr << "[Progress] Completed " << parity << " timestamps.\n";
            break;
        }
        if (parity > 0 && parity % PROGRESS_INTERVAL == 0)
            cerr << "[Progress] " << parity << " timestamps processed (current: " << incomingRow.time << ")\n";
        currentState = policy.Execute(currentState, incomingRow, policyType, migrationPolicyType,arr,FORECAST_WINDOW_SIZE,predictions);
        WorstFitWithRUPredictionPolicy::done=1;
        // if (policyType == 5 and (parity % 36 == 0) and (parity>0))
        // {
        //         cout << "called" << endl;
        //         cout<<"ITERATION:"<<iter<<"\n";
        //         currentState = offlineVro.Execute(currentState, It, Q, THETA_MIN, THETA_MAX);
        //         iter++;
        // }
        parity++;
        globalState.Print(arr, currentState);
    }
    cout << "after making nodestate.csv\n";
    return 0;
}
