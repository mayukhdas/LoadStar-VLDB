#pragma once
#include <string>
#include <boost/multiprecision/cpp_int.hpp>
namespace boost
{
    using int512_t = boost::multiprecision::int512_t;
}
// It is used to store the current load, cpu, memory and storage of a replica
// It contains the replica id, node id, load, cpu, memory and storage requirements
class NodeLoadReplica
{
public:
    std::string ReplicaId;
    int NodeId;
    boost::int512_t Load;
    boost::int512_t  CPU;
    boost::int512_t  Memory;
    boost::int512_t  Storage;
    NodeLoadReplica(){};
    NodeLoadReplica(std::string ReplicaId, int NodeId, boost::int512_t Load,boost::int512_t  CPU,boost::int512_t  Memory, boost::int512_t Storage) : ReplicaId(ReplicaId), NodeId(NodeId), Load(Load), CPU(CPU), Memory(Memory), Storage(Storage) {}
};
