#pragma once
#include <bits/stdc++.h>
#include <cmath>
#include "../common/common.h"
#include "../domain/IncomingRow.hpp"
#include "../domain/Row.hpp"
#include <boost/multiprecision/cpp_int.hpp>
#include <boost/multiprecision/cpp_dec_float.hpp>
#include "../domain/CurrentState.hpp"
#include "../domain/IncomingRow.hpp"
using namespace boost::multiprecision;
using float512 = cpp_dec_float_50;

namespace {
    inline bool isNumeric(const std::string& s) {
        if (s.empty()) return false;
        size_t i = (s[0] == '-' || s[0] == '+') ? 1 : 0;
        for (; i < s.size(); ++i) if (!std::isdigit(static_cast<unsigned char>(s[i])) && s[i] != '.') return false;
        return i > 0;
    }
    inline long long toInt512(const std::string& s) {
        return static_cast<long long>(std::round(std::stod(s)));
    }
    // Normalize id so "1.0" and "1" are the same key (avoids duplicate replica keys and assert in PolicyBase)
    inline std::string normalizeId(const std::string& s) {
        if (s.empty() || !isNumeric(s)) return s;
        try {
            double v = std::stod(s);
            long long i = static_cast<long long>(std::round(v));
            if (std::abs(v - static_cast<double>(i)) < 1e-9) return std::to_string(i);
        } catch (...) {}
        return s;
    }
}

namespace boost
{
    using int512_t = boost::multiprecision::int512_t;
}

// FileReader class to read the incoming data from a CSV file
// The file is expected to be in the format:
// <timestamp>,<UniqueReplicaId>,<PartitionId>,<ReplicaId>,<Load>,<Cpu>,<Memory>,<Storage>
// The class provides a function to read the next timestamp data sequentially
class FileReader
{
public:
    fstream file;
    // Function to read the next timestamp data sequentially
    IncomingRow read_next_timestamp(char* filename, bool optionalArgsForecastFile,bool isWorstFitWithPredictionsAvailable, bool isWorstFitWithCPUMemory)
    {
        vector<string> row;
        string line, word, temp;
        int fileOpen=0;
        if(!file.is_open()){
            char NodeStateFile[500];
            strcpy(NodeStateFile,filename);
            size_t len = strlen(NodeStateFile);
            if (len < 4 || strcmp(NodeStateFile + len - 4, ".csv") != 0)
                strcat(NodeStateFile,".csv");
            file.open(NodeStateFile, ios::in);
            fileOpen=1;
        }
        if (file.eof())
        {
            cout<<"END OF FILE"<<endl;
            file.close();
            IncomingRow incomingRow;
            incomingRow.time = "END OF FILE";
            return incomingRow;
        }
        if (file.is_open())
        {
            IncomingRow incomingRow;
            int reading=0;
            while (getline(file, line))
            {
                row.clear();
                stringstream str(line);
                while (getline(str, word, ','))
                    row.push_back(word);
                const int minCols = isWorstFitWithCPUMemory ? 8 : (optionalArgsForecastFile ? 7 : 5);
                if ((int)row.size() < minCols) continue;
                if (!isNumeric(row[4])) continue;  // skip header or non-data lines
                string time = row[0];
                string partitionId = normalizeId(row[1]);
                string ReplicaId = row[3];
                string UniqueReplicaId = normalizeId(row[2]);
                boost::int512_t Load = toInt512(row[4]);
                // Clamp to per node constraints to avoid issues with predictions being higher than node capacity.
                if (Load > NODE_LOAD) Load = NODE_LOAD -1;

                boost::int512_t  Cpu = 0, Memory = 0, Storage=0, LoadForecasts = 0, CPUForecasts = 0, MemoryForecasts = 0, StorageForecasts =0;
                if(optionalArgsForecastFile){
                    Cpu = toInt512(row[5]);
                    Memory = toInt512(row[6]);
                    // Clamp to per node constraints to avoid issues with predictions being higher than node capacity.
                    if (Cpu > CPU_LOAD) Cpu = CPU_LOAD -1;
                    if (Memory > MEMORY_LOAD) Memory = MEMORY_LOAD -1;
                }
                if(isWorstFitWithPredictionsAvailable){
                    LoadForecasts = toInt512(row[5]);
                }
                if(isWorstFitWithCPUMemory){
                    Cpu = toInt512(row[5]);
                    Memory = toInt512(row[6]);
                    Storage = toInt512(row[7]);
                     // Clamp to per node constraints to avoid issues with predictions being higher than node capacity.
                    if (Cpu > CPU_LOAD) Cpu = CPU_LOAD -1;
                    if (Memory > MEMORY_LOAD) Memory = MEMORY_LOAD -1;
                    if (Storage > STORAGE_LOAD) Storage = STORAGE_LOAD -1;
                }
                if(time!=incomingRow.time and reading==1){
                    // go to previous line and return the incoming row, in case you are at the next timestamp row already 
                    file.seekg(-line.length()-1, ios::cur);
                    return incomingRow;
                }
                incomingRow.time = time;
                incomingRow.Add(UniqueReplicaId, partitionId, Load, ReplicaId, Cpu, Memory,Storage);
                reading =1;
            }
            return incomingRow;
        }
        file.close();
        cout<<"CLOSING FILE"<<endl;
        IncomingRow incomingRow;
        incomingRow.time = "END OF FILE";
        return incomingRow;
    }
};