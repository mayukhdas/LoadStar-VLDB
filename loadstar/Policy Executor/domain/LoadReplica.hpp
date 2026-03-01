#pragma once
#include <string>
using namespace std;
#include <boost/multiprecision/cpp_int.hpp>
namespace boost
{
    using int512_t = boost::multiprecision::int512_t;
}

/* This class is used to store current load and replica id of a replica (Special case of the Ru class)
*/
class LoadReplica
{
public:
    boost::int512_t Load;
    string ReplicaId;
    LoadReplica(){};
    LoadReplica(boost::int512_t Load, string ReplicaId)
    {
        this->Load = Load;
        this->ReplicaId = ReplicaId;
    }
};