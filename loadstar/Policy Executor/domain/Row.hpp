#include <string>
using namespace std;
#include <boost/multiprecision/cpp_int.hpp>
#include <boost/multiprecision/cpp_dec_float.hpp>
namespace boost
{
    using int512_t = boost::multiprecision::int512_t;
    using float512 = boost::multiprecision::cpp_dec_float_50;
}

// This class is used to store the state of the system at a specific time in the simulation.
class Row
{
public:
    string partitionId, ReplicaId;
    string UniqueReplicaId;
    boost::int512_t Load, LoadForecasts, CPUForecasts, MemoryForecasts, StorageForecasts, Cpu, Memory, Storage;
    Row(string partitionId, string ReplicaId, string UniqueReplicaId, boost::int512_t Load, boost::int512_t  Cpu, boost::int512_t  Memory, boost::int512_t  Storage,boost::int512_t LoadForecasts, int512_t CPUForecasts, int512_t MemoryForecasts, int512_t StorageForecasts) : partitionId(partitionId), ReplicaId(ReplicaId), UniqueReplicaId(UniqueReplicaId), Load(Load), Cpu(Cpu), Memory(Memory), Storage(Storage), LoadForecasts(LoadForecasts), CPUForecasts(CPUForecasts), MemoryForecasts(MemoryForecasts) , StorageForecasts(StorageForecasts){}
};