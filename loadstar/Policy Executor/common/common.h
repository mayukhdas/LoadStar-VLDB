#pragma once
#include <boost/multiprecision/cpp_int.hpp>
#include <boost/multiprecision/cpp_dec_float.hpp>
#include <vector>
#include <map>
using namespace std;
namespace boost
{
    using int512_t = boost::multiprecision::int512_t;
    using float512 = boost::multiprecision::cpp_dec_float_50;
}

// Defines load, cpu, memory, storage thresholds for each node
extern boost::int512_t NODE_LOAD;
extern boost::int512_t CPU_LOAD;
extern boost::int512_t MEMORY_LOAD;
extern boost::int512_t STORAGE_LOAD;

// Number of nodes in cluster
extern int NODE_COUNT;

// Defines soft load threshold for each node 
// This is a soft limit, meaning that the system may still place replicas on this node if necessary, but it will try to avoid it.
extern boost::int512_t NODE_LOAD_SOFT;

// Defines thresholds used for Eigen paper simulation
extern boost::int512_t HOTNESS_THRESHOLD;
extern double ALPHA; // factor to multiply the load for loss function
extern double GAMMA; // max deviation from the minimum residual capacity node
extern double THETA_MIN; 
extern double THETA_MAX;
extern boost::float512 Q; // cost factor

// Defines the number of forecasts timestamps ahead to be used for each replica placement
extern int FORECAST_WINDOW_SIZE;

// Defines the alpha and beta parameters for the load balancing algorithm
extern double LOAD_WEIGHT_FACTOR;
extern double SKEWNESS_WEIGHT_FACTOR;

//Defines the class weight matrix for load balancing algorithm
extern vector<vector<int>> compatibilityMatrix;
