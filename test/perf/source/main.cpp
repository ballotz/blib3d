#include "../../source/timer.hpp"
#include "../../source/math.hpp"
#include <cstdio>
#include <cmath>

using namespace blib3d;

timer::profile profile;
constexpr int test_size = 2048;
constexpr int test_repeat = 16;
float test_input[test_size];
float test_reference[test_size];
float test_output[test_size];

void prepare()
{
    for (int i = 0; i < test_size; ++i)
        test_input[i] = static_cast<float>(i + 1) / 128.f;

    for (int i = 0; i < test_size; ++i)
        test_reference[i] = test_input[i];

    for (int i = 0; i < test_size; ++i)
        test_output[i] = test_input[i];
}

template<typename functor>
void test(const char* test_name)
{
    std::printf("%s\n", test_name);

    for (int i = 0; i < test_size; ++i)
        test_reference[i] = functor::reference(test_input[i]);
    
    profile.reset();
    profile.start();

    for (int j = 0; j < test_repeat; ++j)
        for (int i = 0; i < test_size; ++i)
            test_reference[i] = functor::reference(test_input[i]);
    
    profile.stop();
    profile.update();

    std::printf("ref time min %i\n", profile.min());
    std::printf("ref time max %i\n", profile.max());
    std::printf("ref time avg %i\n", profile.avg());

    for (int i = 0; i < test_size; ++i)
        test_output[i] = functor::function(test_input[i]);
    
    profile.reset();
    profile.start();

    for (int j = 0; j < test_repeat; ++j)
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

    std::printf("max err abs %f\n", max_abs_err);
    std::printf("max err rel %f\n\n", max_rel_err);
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

    // static math::powfast_table invsqrt_table;
    // math::powfast_build_table(-0.5f, invsqrt_table);
    // struct invsqrtpowfast
    // {
    //     blib3d_force_inline static float reference(float x) { return 1.f / std::sqrt(x); }
    //     blib3d_force_inline static float function(float x) { return math::powfast(x, invsqrt_table); }
    // };
    // test<invsqrtpowfast>("invsqrtpowfast");

    static math::powfast_table gamma_table;
    math::powfast_build_table(0.45f, gamma_table);
    struct gammapowfast
    {
        blib3d_force_inline static float reference(float x) { return std::pow(x, 0.45f); }
        blib3d_force_inline static float function(float x) { return math::powfast(x, gamma_table); }
    };
    test<gammapowfast>("gammapowfast");

    // struct recip
    // {
    //     blib3d_force_inline static float reference(float x) { return 1.f / x; }
    //     blib3d_force_inline static float function(float x) { return math::recip(x); }
    // };
    // test<recip>("recip");

    // struct sqrt
    // {
    //     blib3d_force_inline static float reference(float x) { return std::sqrt(x); }
    //     blib3d_force_inline static float function(float x) { return math::sqrt(x); }
    // };
    // test<sqrt>("sqrt");

    // struct log2
    // {
    //     blib3d_force_inline static float reference(float x) { return std::log2(x); }
    //     blib3d_force_inline static float function(float x) { return math::log2(x); }
    // };
    // test<log2>("log2");

    // struct invsqrt
    // {
    //     blib3d_force_inline static float reference(float x) { return 1.f / std::sqrt(x); }
    //     blib3d_force_inline static float function(float x) { return math::invsqrt(x); }
    // };
    // test<invsqrt>("invsqrt");
}
