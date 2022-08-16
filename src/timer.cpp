#include "timer.hpp"

namespace lib3d::timer
{

void profile::reset()
{
    us = 0;
    us_min = UINT32_MAX;
    us_max = 0;
    us_avg = 0.f;
    count = 0.f;
}

void profile::update()
{
    us_min = us < us_min ? us : us_min;
    us_max = us > us_max ? us : us_max;
    us_avg = (us_avg * count + (float)us) / (count + 1.f);
    count += 1.f;
    us = 0;
}

//------------------------------------------------------------------------------

void interval::reset()
{
    t = std::chrono::high_resolution_clock::now();
    us = 0;
}

} // namespace lib3d::timer
