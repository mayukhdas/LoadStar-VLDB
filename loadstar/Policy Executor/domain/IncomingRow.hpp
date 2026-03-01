#pragma once
#include <iostream>
#include <map>
#include <vector>
#include "LoadReplica.hpp"
#include "d-RU.hpp"
#include <boost/multiprecision/cpp_int.hpp>
#include <boost/multiprecision/cpp_dec_float.hpp>
using namespace boost::multiprecision;
using float512 = cpp_dec_float_50;
namespace boost
{
    using int512_t = boost::multiprecision::int512_t;
}
using namespace std;

/* 
This class is used to store the incoming row of data from the CSV file for a timestamp. 
It contains the unique replica id, partition id, load, replica id, cpu, memory and storage requirements.
It also contains the methods to add the data to the class and print the data.
*/
class IncomingRow
{
public:
    // stores the <UniqueReplicaId, PartitionId> pair
    vector<pair<string, string>> uriPartition;

    // maps <UniqueReplicaId, PartitionId> pair to its load and replica id
    map<pair<string, string>, LoadReplica> uriPartitionLoadReplicaMap;
    
    // stores timestamp
    string time;
    
    // maps <UniqueReplicaId, PartitionId> pair to its load, cpu, memory and storage requirements
    map<pair<string, string>, Ru> uriPartitionRuMap;

    IncomingRow(string time = "") : time(time) {}

    void Add(string _uniqueReplicaId, string _partitionId, boost::int512_t _load, string _replicaId, boost::int512_t  _cpu, boost::int512_t  _memory, boost::int512_t  _storage)
    {
        uriPartition.push_back({_uniqueReplicaId, _partitionId});
        uriPartitionLoadReplicaMap[{_uniqueReplicaId, _partitionId}] = LoadReplica(_load, _replicaId);
        uriPartitionRuMap[{_uniqueReplicaId, _partitionId}] = Ru(_cpu, _memory, _storage);
    }

    void Print()
    {
        for (auto it : uriPartition)
        {
            cout << it.first << " " << it.second << " ";
            cout<< uriPartitionRuMap[{it.first,it.second}].Cpu<<" "<<uriPartitionRuMap[{it.first,it.second}].Cpu<<endl;
        }
    }
};