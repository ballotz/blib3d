#pragma once
#include <chrono>
#include <cstdint>

namespace lib3d::timer
{

class profile
{
public:
    inline void start()
    {
        t = std::chrono::high_resolution_clock::now();
    }

    inline void stop()
    {
        us += (uint32_t)std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::high_resolution_clock::now() - t).count();
    }

    void reset();
    void update();

    uint32_t min() { return us_min; };
    uint32_t max() { return us_max; };
    uint32_t avg() { return (uint32_t)us_avg; };

private:
    std::chrono::time_point<std::chrono::high_resolution_clock> t;
    uint32_t us;
    uint32_t us_min;
    uint32_t us_max;
    float us_avg;
    float count;
};

//------------------------------------------------------------------------------

class interval
{
public:
    inline void init()
    {
        us = 0;
        t = std::chrono::high_resolution_clock::now();
    }

    inline void tick()
    {
        auto new_t = std::chrono::high_resolution_clock::now();
        us = (uint32_t)std::chrono::duration_cast<std::chrono::microseconds>(new_t - t).count();
        t = new_t;
    }

    void reset();

    uint32_t get_us() { return us; }
    float get_s() { return (float)us * 1e-6f; }

private:
    std::chrono::time_point<std::chrono::high_resolution_clock> t;
    uint32_t us;
};

} // namespace lib3d::timer
