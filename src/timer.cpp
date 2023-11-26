#include "timer.hpp"

namespace lib3d::timer
{

void profile::reset()
{
#if defined(CPU_MIMXRT1176DVMAA)
    e = 0;
#else
    e = duration_type::zero();
#endif
    us_min = UINT32_MAX;
    us_max = 0;
    us_avg = 0.f;
    count = 0.f;
}

void profile::update()
{
#if defined(CPU_MIMXRT1176DVMAA)
    uint32_t us{ (uint32_t)((float)e * (1e6f / (float)SystemCoreClock)) };
    e = 0;
#else
    uint32_t us{ (uint32_t)std::chrono::duration_cast<std::chrono::microseconds>(e).count() };
    e = duration_type::zero();
#endif

    us_min = us < us_min ? us : us_min;
    us_max = us > us_max ? us : us_max;
    us_avg = (us_avg * count + (float)us) / (count + 1.f);
    count += 1.f;
}

//------------------------------------------------------------------------------

void interval::reset()
{
#if defined(CPU_MIMXRT1176DVMAA)
    t = DWT->CYCCNT;
#else
    t = std::chrono::high_resolution_clock::now();
#endif
    us = 0;
}

} // namespace lib3d::timer
