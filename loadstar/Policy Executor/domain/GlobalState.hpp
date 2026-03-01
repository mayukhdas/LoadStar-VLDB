#include "CurrentState.hpp"
#include <vector>
#include <boost/multiprecision/cpp_int.hpp>
namespace boost
{
    using int512_t = boost::multiprecision::int512_t;
}

/* This class is used to store the global state of the system across entire simulation time.
 It contains a vector of CurrentState objects, each representing the state of the system at a specific time.
*/
class GlobalState
{
public:
    vector<CurrentState> StateMap;
    void Add(CurrentState &state)
    {
        StateMap.push_back(state);
    }
    void Print(char * fileprefix, CurrentState &currentState)
    {
        std::ofstream myfile;
        
        char NodeStateFile[500],ReplicaStateFile[500];
        strcpy(NodeStateFile,fileprefix);
        strcat(NodeStateFile,"_NodeState.csv");
        myfile.open(NodeStateFile, std::ios_base::app);
        currentState.Print(NodeStateFile,ReplicaStateFile);

        myfile.close();
    }
};