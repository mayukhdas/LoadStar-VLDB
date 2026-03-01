#include <map>
using namespace std;
#include <boost/multiprecision/cpp_int.hpp>
namespace boost
{
    using int512_t = boost::multiprecision::int512_t;
}
/* 
 It is used to store the migration details of a replica
 It contains the old node id, new node id, replica id and a flag 
 to indicate if the migration is based on forecasts or contraint violations.
*/
class MigrationDetail
{
public:
    int oldNodeId, newNodeId;
    string ReplicaId;
    bool Flag;
};
class MigrationInfo
{
public:
    map<pair<int, string>, MigrationDetail> MigrationInfoMap;
};