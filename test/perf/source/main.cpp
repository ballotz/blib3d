#include "../../../source/timer.hpp"
#include "../../../source/math.hpp"
#include <cstdio>
#include <cmath>
#include <cstring>

#if defined(BLIB3D_ARCH_SSE)
#include <xmmintrin.h>
#endif

#if defined(BLIB3D_ARCH_NEON)
#include <arm_neon.h>
#endif

using namespace blib3d;

//------------------------------------------------------------------------------
// invsqrt variants under test (x > 0 assumed)

namespace invsqrt_impl
{

#if defined(BLIB3D_ARCH_SSE)

// hardware estimate, ~12 bit (rel err <= 1.5 * 2^-12)
blib3d_force_inline float rsqrtss(float x)
{
    return _mm_cvtss_f32(_mm_rsqrt_ss(_mm_set_ss(x)));
}

// hardware estimate + one Newton-Raphson step, ~22-23 bit
blib3d_force_inline float rsqrtss_nr1(float x)
{
    float y{ _mm_cvtss_f32(_mm_rsqrt_ss(_mm_set_ss(x))) };
    y *= 1.5f - (0.5f * x) * y * y;
    return y;
}

#endif

#if defined(BLIB3D_ARCH_NEON)

// hardware estimate, ~8 bit
blib3d_force_inline float frsqrte(float x)
{
    return vrsqrtes_f32(x);
}

// estimate + one frsqrts step, ~16 bit
blib3d_force_inline float frsqrte_nr1(float x)
{
    float y{ vrsqrtes_f32(x) };
    y *= vrsqrtss_f32(x * y, y);
    return y;
}

// estimate + two frsqrts steps, ~23 bit
blib3d_force_inline float frsqrte_nr2(float x)
{
    float y{ vrsqrtes_f32(x) };
    y *= vrsqrtss_f32(x * y, y);
    y *= vrsqrtss_f32(x * y, y);
    return y;
}

#endif

} // namespace invsqrt_impl

timer::profile profile;
constexpr int test_size = 32768;
constexpr int test_repeat = 32;
constexpr float test_min = 1 / 128.f;
constexpr float test_max = 16.f;
float test_input[test_size];
float test_reference[test_size];
float test_output[test_size];
volatile float test_sink;

void prepare()
{
    for (int i = 0; i < test_size; ++i)
    {
        float a = static_cast<float>(i) / static_cast<float>(test_size);
        test_input[i] = test_min * (1.f - a) + test_max * a;
    }

    for (int i = 0; i < test_size; ++i)
        test_reference[i] = test_input[i];

    for (int i = 0; i < test_size; ++i)
        test_output[i] = test_input[i];
}

// auto-vectorization disabled, so reference and function are compared scalar vs scalar:
// - throughput: independent calls
// - latency: every call depends on the previous result through 'x + y * 0.f' (not foldable without fast-math)
//   the chain adds the same mul+add latency to every functor

#if defined(_MSC_VER) && !defined(__clang__)
#define PERF_NO_VECTORIZE_LOOP __pragma(loop(no_vector))
#define PERF_NO_VECTORIZE_FN
#elif defined(__clang__)
#define PERF_NO_VECTORIZE_LOOP _Pragma("clang loop vectorize(disable) interleave(disable)")
#define PERF_NO_VECTORIZE_FN
#elif defined(__GNUC__)
#define PERF_NO_VECTORIZE_LOOP
#define PERF_NO_VECTORIZE_FN __attribute__((optimize("no-tree-vectorize")))
#else
#define PERF_NO_VECTORIZE_LOOP
#define PERF_NO_VECTORIZE_FN
#endif

template<typename functor>
PERF_NO_VECTORIZE_FN
void test(const char* test_name)
{
    float test_v = test_sink;

    std::printf("%s\n", test_name);

    for (int i = 0; i < test_size; ++i)
        test_reference[i] = functor::reference(test_input[i]);
    
    profile.reset();
    for (int j = 0; j < test_repeat; ++j)
    {
        profile.start();
        PERF_NO_VECTORIZE_LOOP
        for (int i = 0; i < test_size; ++i)
            test_reference[i] = functor::reference(test_input[i]);
        profile.stop();
        profile.update();
    }

    std::printf("ref throughput %i\n", profile.min());

    profile.reset();
    for (int j = 0; j < test_repeat; ++j)
    {
        profile.start();
        PERF_NO_VECTORIZE_LOOP
        for (int i = 0; i < test_size; ++i)
            test_v = functor::reference(test_input[i] + test_v * 0.f);
        profile.stop();
        profile.update();
    }

    std::printf("ref latency %i\n", profile.min());

    for (int i = 0; i < test_size; ++i)
        test_output[i] = functor::function(test_input[i]);
    
    profile.reset();
    for (int j = 0; j < test_repeat; ++j)
    {
        profile.start();
        PERF_NO_VECTORIZE_LOOP
        for (int i = 0; i < test_size; ++i)
            test_output[i] = functor::function(test_input[i]);    
        profile.stop();
        profile.update();
    }

    std::printf("func throughput %i\n", profile.min());

    profile.reset();
    for (int j = 0; j < test_repeat; ++j)
    {
        profile.start();
        PERF_NO_VECTORIZE_LOOP
        for (int i = 0; i < test_size; ++i)
            test_v = functor::function(test_input[i] + test_v * 0.f);
        profile.stop();
        profile.update();
    }

    std::printf("func latency %i\n", profile.min());

    float max_abs_err = 0.f;
    float max_rel_err = 0.f;
    for (int i = 0; i < test_size; ++i)
    {
        float abs_err = std::abs(test_reference[i] - test_output[i]);
        if (max_abs_err < abs_err)
            max_abs_err = abs_err;
        // if (abs_err == 1.f)
        //     std::printf("value %f\n", test_input[i]);
        if (test_reference[i] != 0.f)
        {
            float rel_err = abs_err / test_reference[i];
            if (max_rel_err < rel_err)
                max_rel_err = rel_err;
        }
    }

    std::printf("max err abs %e\n", max_abs_err);
    std::printf("max err rel %e\n\n", max_rel_err);

    test_sink = test_v;
}

int main()
{
    prepare();

    struct log2floor
    {
        blib3d_force_inline static float reference(float x) { return std::floor(std::log2(x)); }
        blib3d_force_inline static float function(float x) { return math::log2floor(x); }
    };
    test<log2floor>("log2floor");

    struct log2ceil
    {
        blib3d_force_inline static float reference(float x) { return std::ceil(std::log2(x)); }
        blib3d_force_inline static float function(float x) { return math::log2ceil(x); }
    };
    test<log2ceil>("log2ceil");

    // round(log2(5792.617188)) = 12
    struct log2round
    {
        blib3d_force_inline static float reference(float x) { return std::round(std::log2(x)); }
        blib3d_force_inline static float function(float x) { return math::log2round(x); }
    };
    test<log2round>("log2round");

    struct log2fast
    {
        blib3d_force_inline static float reference(float x) { return std::log2(x); }
        blib3d_force_inline static float function(float x) { return math::log2fast(x); }
    };
    test<log2fast>("log2fast");

    // struct recipfast
    // {
    //     blib3d_force_inline static float reference(float x) { return 1.f / x; }
    //     blib3d_force_inline static float function(float x) { return math::recipfast(x); }
    // };
    // test<recipfast>("recipfast");

    // struct sqrtfast
    // {
    //     blib3d_force_inline static float reference(float x) { return std::sqrt(x); }
    //     blib3d_force_inline static float function(float x) { return math::sqrtfast(x); }
    // };
    // test<sqrtfast>("sqrtfast");

    // struct invsqrtfast
    // {
    //     blib3d_force_inline static float reference(float x) { return 1.f / std::sqrt(x); }
    //     blib3d_force_inline static float function(float x) { return math::invsqrtfast(x); }
    // };
    // test<invsqrtfast>("invsqrtfast");

    // static math::powfast_table sqrt_table;
    // math::powfast_build_table(0.5f, sqrt_table);
    // struct sqrtpowfast
    // {
    //     blib3d_force_inline static float reference(float x) { return std::sqrt(x); }
    //     blib3d_force_inline static float function(float x) { return math::powfast(x, sqrt_table); }
    // };
    // test<sqrtpowfast>("sqrtpowfast");

    static math::powfast_table invsqrt_table;
    math::powfast_build_table(-0.5f, invsqrt_table);
    struct invsqrtpowfast
    {
        blib3d_force_inline static float reference(float x) { return 1.f / std::sqrt(x); }
        blib3d_force_inline static float function(float x) { return math::powfast(x, invsqrt_table); }
    };
    test<invsqrtpowfast>("invsqrtpowfast");

    static math::powfast_table gamma_table;
    math::powfast_build_table(0.45f, gamma_table);
    struct gammapowfast
    {
        blib3d_force_inline static float reference(float x) { return std::pow(x, 0.45f); }
        blib3d_force_inline static float function(float x) { return math::powfast(x, gamma_table); }
    };
    test<gammapowfast>("gammapowfast");

    struct log2
    {
        blib3d_force_inline static float reference(float x) { return std::log2(x); }
        blib3d_force_inline static float function(float x) { return math::log2(x); }
    };
    test<log2>("log2");

    struct recip
    {
        blib3d_force_inline static float reference(float x) { return 1.f / x; }
        blib3d_force_inline static float function(float x) { return math::recip(x); }
    };
    test<recip>("recip");

    struct sqrt
    {
        blib3d_force_inline static float reference(float x) { return std::sqrt(x); }
        blib3d_force_inline static float function(float x) { return math::sqrt(x); }
    };
    test<sqrt>("sqrt");

    struct invsqrt
    {
        blib3d_force_inline static float reference(float x) { return 1.f / std::sqrt(x); }
        blib3d_force_inline static float function(float x) { return math::invsqrt(x); }
    };
    test<invsqrt>("invsqrt");

#if defined(BLIB3D_ARCH_SSE)
    struct invsqrt_rsqrtss
    {
        blib3d_force_inline static float reference(float x) { return 1.f / std::sqrt(x); }
        blib3d_force_inline static float function(float x) { return invsqrt_impl::rsqrtss(x); }
    };
    test<invsqrt_rsqrtss>("invsqrt rsqrtss");

    struct invsqrt_rsqrtss_nr1
    {
        blib3d_force_inline static float reference(float x) { return 1.f / std::sqrt(x); }
        blib3d_force_inline static float function(float x) { return invsqrt_impl::rsqrtss_nr1(x); }
    };
    test<invsqrt_rsqrtss_nr1>("invsqrt rsqrtss + 1 NR");
#endif

#if defined(BLIB3D_ARCH_NEON)
    struct invsqrt_frsqrte
    {
        blib3d_force_inline static float reference(float x) { return 1.f / std::sqrt(x); }
        blib3d_force_inline static float function(float x) { return invsqrt_impl::frsqrte(x); }
    };
    test<invsqrt_frsqrte>("invsqrt frsqrte");

    struct invsqrt_frsqrte_nr1
    {
        blib3d_force_inline static float reference(float x) { return 1.f / std::sqrt(x); }
        blib3d_force_inline static float function(float x) { return invsqrt_impl::frsqrte_nr1(x); }
    };
    test<invsqrt_frsqrte_nr1>("invsqrt frsqrte + 1 frsqrts");

    struct invsqrt_frsqrte_nr2
    {
        blib3d_force_inline static float reference(float x) { return 1.f / std::sqrt(x); }
        blib3d_force_inline static float function(float x) { return invsqrt_impl::frsqrte_nr2(x); }
    };
    test<invsqrt_frsqrte_nr2>("invsqrt frsqrte + 2 frsqrts");
#endif
}
