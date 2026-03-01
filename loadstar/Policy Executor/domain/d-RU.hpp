#pragma once
using namespace std;
#include <boost/multiprecision/cpp_dec_float.hpp>
namespace boost
{
    using int512_t = boost::multiprecision::int512_t;
    using float512 = boost::multiprecision::cpp_dec_float_50;
}

/* This class is used to represent the resource utilization (RU) of a node in a distributed system.
 It contains three main attributes: CPU, Memory, and Storage, each represented as a 512-bit integer.
 The class provides a constructor to initialize these attributes and 
 a method to calculate the total RU by summing up the individual components.
*/

class Ru
{
public:
    boost::int512_t  Cpu, Memory, Storage;
    Ru()
    {
        Cpu = 0;
        Memory = 0;
        Storage = 0;
    };
    Ru(boost::int512_t  Cpu, boost::int512_t  Memory, boost::int512_t Storage)
    {
        this->Cpu = Cpu;
        this->Memory = Memory;
        this->Storage = Storage;
    }
    boost::int512_t  getRu()
    {
        return Cpu + Memory + Storage;
    }
};