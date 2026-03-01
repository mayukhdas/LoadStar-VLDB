#pragma once
#include <iostream>
#include <boost/multiprecision/cpp_int.hpp>
#include <boost/multiprecision/cpp_dec_float.hpp>
#include "ForecastValue.hpp"
namespace boost
{
    using int512_t = boost::multiprecision::int512_t;
    using float512 = boost::multiprecision::cpp_dec_float_50;
}
using namespace std;
using namespace boost;

/* This class is used to store the maximum forecast value of a replica at a specific time in the simulation.
 It contains the forecast value, cpu time, memory time, load time and storage time.
 It also contains the methods to add the data to the class and print the data.
*/
class MaxForecast
{
public:
    ForecastValue MaxForecastValue;
    string CpuTime, MemoryTime, LoadTime, StorageTime;
    MaxForecast(ForecastValue MaxForecastValue, string CpuTime, string MemoryTime, string StorageTime, string LoadTime) : MaxForecastValue(MaxForecastValue), CpuTime(CpuTime), MemoryTime(MemoryTime), StorageTime(StorageTime), LoadTime(LoadTime) {}
    MaxForecast() : MaxForecastValue(ForecastValue()), CpuTime(""), MemoryTime(""), StorageTime(""), LoadTime("") {}
    MaxForecast(string time){
        CpuTime = time;
        MemoryTime = time;
        StorageTime = time;
        LoadTime = time;
    }
};
