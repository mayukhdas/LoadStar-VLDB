#pragma once
#include <iostream>
#include <boost/multiprecision/cpp_int.hpp>
#include <boost/multiprecision/cpp_dec_float.hpp>
namespace boost
{
    using int512_t = boost::multiprecision::int512_t;
    using float512 = boost::multiprecision::cpp_dec_float_50;
}
using namespace std;
using namespace boost;

/* This class is used to store the forecasted resource utilization (RU) values for a node.
It contains four main attributes: ForecastLoad, ForecastCPU, ForecastMemory, and ForecastStorage, 
each represented as a 512-bit integer. The class provides a constructor to initialize these attributes and 
methods to add and delete forecast values, whenever a replica moves in/out of a node as per the policy.
*/

class ForecastValue
{
public:
    boost::int512_t ForecastLoad, ForecastCPU, ForecastMemory, ForecastStorage;
    ForecastValue() : ForecastLoad(0), ForecastCPU(0), ForecastMemory(0), ForecastStorage(0) {}
    void Add(ForecastValue &forecastValue)
    {
        ForecastLoad += forecastValue.ForecastLoad;
        ForecastCPU += forecastValue.ForecastCPU;
        ForecastMemory += forecastValue.ForecastMemory;
        ForecastStorage += forecastValue.ForecastStorage;
    }
    void Delete(ForecastValue &forecastValue)
    {
        ForecastLoad -= forecastValue.ForecastLoad;
        ForecastCPU -= forecastValue.ForecastCPU;
        ForecastMemory -= forecastValue.ForecastMemory;
        ForecastStorage -= forecastValue.ForecastStorage;
    }
    ForecastValue(int512_t ForecastLoad, int512_t ForecastCPU, int512_t ForecastMemory, int512_t ForecastStorage) : ForecastLoad(ForecastLoad), ForecastCPU(ForecastCPU), ForecastMemory(ForecastMemory), ForecastStorage(ForecastStorage) {}
};
