#pragma once
#if defined(CPU_MIMXRT1176DVMAA_cm7)
#include "MIMXRT1176_cm7.h"
#include "MIMXRT1176_cm7_features.h"
#else
#include <chrono>
#endif
#include <cstdint>

namespace lib3d::timer
{

#if defined(CPU_MIMXRT1176DVMAA)
using time_point_type = uint32_t;
using duration_type = uint32_t;
#else
using time_point_type = std::chrono::high_resolution_clock::time_point;
using duration_type = std::chrono::high_resolution_clock::duration;
#endif

//------------------------------------------------------------------------------

class profile
{
public:
    inline void start()
    {
#if defined(CPU_MIMXRT1176DVMAA)
        t = DWT->CYCCNT;
#else
        t = std::chrono::high_resolution_clock::now();
#endif
    }

    inline void stop()
    {
#if defined(CPU_MIMXRT1176DVMAA)
        e += DWT->CYCCNT - t;
#else
        e += std::chrono::high_resolution_clock::now() - t;
#endif
    }

    void reset();
    void update();

    uint32_t min() { return us_min; };
    uint32_t max() { return us_max; };
    uint32_t avg() { return (uint32_t)us_avg; };

private:
    time_point_type t;
    duration_type e;
    uint32_t us_min;
    uint32_t us_max;
    float us_avg;
    float count;
};

//------------------------------------------------------------------------------

class interval
{
public:
    inline void tick()
    {
#if defined(CPU_MIMXRT1176DVMAA)
        time_point_type new_t = DWT->CYCCNT;
        us = (uint32_t)((float)(new_t - t) * (1e6f / (float)SystemCoreClock));
#else
        time_point_type new_t = std::chrono::high_resolution_clock::now();
        us = (uint32_t)std::chrono::duration_cast<std::chrono::microseconds>(new_t - t).count();
#endif
        t = new_t;
    }

    void reset();

    uint32_t get_us() { return us; }
    float get_s() { return (float)us * 1e-6f; }

private:
    time_point_type t;
    uint32_t us;
};

} // namespace lib3d::timer
