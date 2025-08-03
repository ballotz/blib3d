#include "../../src/timer.hpp"
#include "../../src/math.hpp"
#include <cstdio>
#include <cmath>

using namespace blib3d;

timer::profile profile;
constexpr int test_size = 0x10000;
float test_input[test_size];
float test_reference[test_size];
float test_output[test_size];

void prepare()
{
    for (int i = 0; i < test_size; ++i)
        test_input[i] = static_cast<float>(i + 1) / 7.f;

    for (int i = 0; i < test_size; ++i)
        test_reference[i] = test_input[i];

    for (int i = 0; i < test_size; ++i)
        test_output[i] = test_input[i];
}

template<typename functor>
void test(const char* test_name)
{
    std::printf("%s\n", test_name);

    profile.reset();
    profile.start();

    for (int i = 0; i < test_size; ++i)
        test_reference[i] = functor::reference(test_input[i]);
    
    profile.stop();
    profile.update();

    std::printf("ref time min %i\n", profile.min());
    std::printf("ref time max %i\n", profile.max());
    std::printf("ref time avg %i\n", profile.avg());

    profile.reset();
    profile.start();

    for (int i = 0; i < test_size; ++i)
        test_output[i] = functor::function(test_input[i]);
    
    profile.stop();
    profile.update();

    std::printf("func time min %i\n", profile.min());
    std::printf("func time max %i\n", profile.max());
    std::printf("func time avg %i\n", profile.avg());

    float max_abs_err = 0.f;
    float max_rel_err = 0.f;
    for (int i = 0; i < test_size; ++i)
    {
        float abs_err = std::abs(test_reference[i] - test_output[i]);
        float rel_err = abs_err / test_reference[i];
        if (max_abs_err < abs_err)
            max_abs_err = abs_err;
        if (max_rel_err < rel_err)
            max_rel_err = rel_err;
    }

    std::printf("max err abs %f\n", max_abs_err);
    std::printf("max err rel %f\n\n", max_rel_err);
}

int main()
{
    prepare();

    // struct recipfast
    // {
    //     force_inline static float reference(float x) { return 1.f / x; }
    //     force_inline static float function(float x) { return math::recipfast(x); }
    // };
    // test<recipfast>("recipfast");

    struct recip
    {
        force_inline static float reference(float x) { return 1.f / x; }
        force_inline static float function(float x) { return math::recip(x); }
    };
    test<recip>("recip");

    // struct sqrtfast
    // {
    //     force_inline static float reference(float x) { return std::sqrt(x); }
    //     force_inline static float function(float x) { return math::sqrtfast(x); }
    // };
    // test<sqrtfast>("sqrtfast");

    struct sqrt
    {
        force_inline static float reference(float x) { return std::sqrt(x); }
        force_inline static float function(float x) { return math::sqrt(x); }
    };
    test<sqrt>("sqrt");

    struct log2fast
    {
        force_inline static float reference(float x) { return std::log2(x); }
        force_inline static float function(float x) { return math::log2fast(x); }
    };
    test<log2fast>("log2fast");

    struct log2
    {
        force_inline static float reference(float x) { return std::log2(x); }
        force_inline static float function(float x) { return math::log2(x); }
    };
    test<log2>("log2");

    struct invsqrt
    {
        force_inline static float reference(float x) { return 1.f / std::sqrt(x); }
        force_inline static float function(float x) { return math::invsqrt(x); }
    };
    test<invsqrt>("invsqrt");
}
