#pragma once
#include "shared.hpp"
#include <cmath>

#if defined(ARCH_INTEL) && defined(USE_SIMD)
#include <xmmintrin.h>
#endif

namespace lib3d::math
{

// constants

constexpr float pi{ (float)3.1415926535897932384626433832795 };
constexpr float pi2{ (float)6.283185307179586476925286766559 };

// types

typedef float vec3[3];
typedef float vec4[4];

typedef float mat3x3[9]; // 00, 01, 02, 10, 11, 12, 20, 21, 22
typedef float mat4x4[16]; // 00, 01, 02, 03, 10, 11, 12, 13, 20, 21, 22, 23, 30, 31, 32, 33

// common

template<typename data_type>
inline data_type min(data_type v0, data_type v1)
{
    return v0 < v1 ? v0 : v1;
}

template<typename data_type>
inline data_type max(data_type v0, data_type v1)
{
    return v0 > v1 ? v0 : v1;
}

template<typename data_type>
inline data_type clamp(data_type v, data_type vmin, data_type vmax)
{
    return min(max(v, vmin), vmax);
}

inline int32_t floor(float v)
{
    int32_t r{ (int32_t)v };
    if ((float)r > v)
        --r;
    return r;
}

inline int32_t ceil(float v)
{
    int32_t r{ (int32_t)v };
    if ((float)r < v)
        ++r;
    return r;
}

// bits

// is_power_of_2(0) = false
inline uint32_t is_power_of_2(uint32_t v)
{
    return v && !(v & (v - 1u));
}

inline uint32_t next_power_of_2(uint32_t v)
{
    v--;
    v |= v >> 1u;
    v |= v >> 2u;
    v |= v >> 4u;
    v |= v >> 8u;
    v |= v >> 16u;
    v++;
    return v;
}

inline uint32_t prev_power_of_2(uint32_t v)
{
    v |= v >> 1u;
    v |= v >> 2u;
    v |= v >> 4u;
    v |= v >> 8u;
    v |= v >> 16u;
    v -= v >> 1u;
    return v;
}

// assume v is positive
inline int32_t log2floor(float v)
{
    return (reinterpret_cast<int32_t&>(v) >> 23) - 127;
}

// assume v is positive
inline int32_t log2ceil(float v)
{
    int32_t i{ reinterpret_cast<int32_t&>(v) };
    int32_t r{ (i >> 23) - 127 };
    r += (i & 0x007FFFFF) != 0;
    return r;
}

// assume v is positive
inline int32_t log2round(float v)
{
    int32_t i{ reinterpret_cast<int32_t&>(v) };
    int32_t r{ (i >> 23) - 127 };
    r += (i & 0x007FFFFF) >= 3474675;
    return r;
}

// assume v is positive
inline float log2fast(float v)
{
    constexpr float scale{ 1.f / (1 << 23) };
    return (float)reinterpret_cast<int32_t&>(v) * scale - 127.f;
}

// assume v is positive
inline float sqrtfast(float v)
{
    int32_t r{ (reinterpret_cast<int32_t&>(v) >> 1) + 0x1FC00000 };
    return reinterpret_cast<float&>(r);
}

inline float sqrt(float v)
{
#if 1
    return std::sqrt(v);
#endif
#if 0
    float r{ sqrtfast(v) };
    r = 0.5f * (r + v / r);
    r = 0.5f * (r + v / r);
    return r;
#endif
}

inline float log2(float v)
{
    return std::log2(v);
}

inline int32_t log2(int32_t v)
{
    return log2floor((float)v);
}

inline float invsqrt(float v)
{
#if 1
    return 1.f / std::sqrt(v);
#endif
#if 0
    float vh{ v * 0.5f };
    reinterpret_cast<uint32_t&>(v) = 0x5F3759DFu - (reinterpret_cast<uint32_t&>(v) >> 1u);
    v *= 1.5f - vh * v * v;
    v *= 1.5f - vh * v * v;
    return v;
#endif
}

// copy

inline void copy3(vec3 out, const vec3 in)
{
    out[0] = in[0];
    out[1] = in[1];
    out[2] = in[2];
}

inline void copy4(vec3 out, const vec3 in)
{
#if defined(ARCH_INTEL) && defined(USE_SIMD)
    _mm_storeu_ps(out, _mm_loadu_ps(in));
#else
    out[0] = in[0];
    out[1] = in[1];
    out[2] = in[2];
    out[3] = in[3];
#endif
}

inline void copy3x3(mat3x3 out, const mat3x3 in)
{
#if defined(ARCH_INTEL) && defined(USE_SIMD)
    _mm_storeu_ps(out + 0, _mm_loadu_ps(in + 0));
    _mm_storeu_ps(out + 4, _mm_loadu_ps(in + 4));
#else
    out[0] = in[0];
    out[1] = in[1];
    out[2] = in[2];
    out[3] = in[3];
    out[4] = in[4];
    out[5] = in[5];
    out[6] = in[6];
    out[7] = in[7];
#endif
    out[8] = in[8];
}

inline void copy4x4(mat4x4 out, const mat4x4 in)
{
#if defined(ARCH_INTEL) && defined(USE_SIMD)
    _mm_storeu_ps(out +  0, _mm_loadu_ps(in +  0));
    _mm_storeu_ps(out +  4, _mm_loadu_ps(in +  4));
    _mm_storeu_ps(out +  8, _mm_loadu_ps(in +  8));
    _mm_storeu_ps(out + 12, _mm_loadu_ps(in + 12));
#else
    out[ 0] = in[ 0];
    out[ 1] = in[ 1];
    out[ 2] = in[ 2];
    out[ 3] = in[ 3];
    out[ 4] = in[ 4];
    out[ 5] = in[ 5];
    out[ 6] = in[ 6];
    out[ 7] = in[ 7];
    out[ 8] = in[ 8];
    out[ 9] = in[ 9];
    out[10] = in[10];
    out[11] = in[11];
    out[12] = in[12];
    out[13] = in[13];
    out[14] = in[14];
    out[15] = in[15];
#endif
}

// add

inline void add3(vec3 inout, const vec3 a)
{
    inout[0] += a[0];
    inout[1] += a[1];
    inout[2] += a[2];
}

// sub

inline void sub3(vec3 out, const vec3 a, const vec3 b)
{
    out[0] = a[0] - b[0];
    out[1] = a[1] - b[1];
    out[2] = a[2] - b[2];
}

// mul

inline void mul3(vec3 inout, const float a)
{
    inout[0] *= a;
    inout[1] *= a;
    inout[2] *= a;
}

inline void mul4(vec4 inout, const float a)
{
#if defined(ARCH_INTEL) && defined(USE_SIMD)
    _mm_storeu_ps(inout, _mm_mul_ps(_mm_loadu_ps(inout), _mm_load1_ps(&a)));
#else
    inout[0] *= a;
    inout[1] *= a;
    inout[2] *= a;
    inout[3] *= a;
#endif
}

inline void mul3(vec3 out, const vec3 a, const float b)
{
    out[0] = a[0] * b;
    out[1] = a[1] * b;
    out[2] = a[2] * b;
}

inline void mul3x3_3(vec3 out, const mat3x3 a, const vec3 b)
{
    out[0] = a[0] * b[0] + a[1] * b[1] + a[2] * b[2];
    out[1] = a[3] * b[0] + a[4] * b[1] + a[5] * b[2];
    out[2] = a[6] * b[0] + a[7] * b[1] + a[8] * b[2];
}

inline void mul3x3t_3(vec3 out, const mat3x3 a, const vec3 b)
{
    out[0] = a[0] * b[0] + a[3] * b[1] + a[6] * b[2];
    out[1] = a[1] * b[0] + a[4] * b[1] + a[7] * b[2];
    out[2] = a[2] * b[0] + a[5] * b[1] + a[8] * b[2];
}

inline void mul3x3_3x3(mat3x3 out, const mat3x3 a, const mat3x3 b)
{
    out[0] = a[0] * b[0] + a[1] * b[3] + a[2] * b[6];
    out[1] = a[0] * b[1] + a[1] * b[4] + a[2] * b[7];
    out[2] = a[0] * b[2] + a[1] * b[5] + a[2] * b[8];
    out[3] = a[3] * b[0] + a[4] * b[3] + a[5] * b[6];
    out[4] = a[3] * b[1] + a[4] * b[4] + a[5] * b[7];
    out[5] = a[3] * b[2] + a[4] * b[5] + a[5] * b[8];
    out[6] = a[6] * b[0] + a[7] * b[3] + a[8] * b[6];
    out[7] = a[6] * b[1] + a[7] * b[4] + a[8] * b[7];
    out[8] = a[6] * b[2] + a[7] * b[5] + a[8] * b[8];
}

inline void mul4x4t_3(vec4 out, const mat4x4 a, const vec3 b)
{
#if defined(ARCH_INTEL) && defined(USE_SIMD)
    __m128 t0{ _mm_mul_ps(_mm_loadu_ps(a + 0), _mm_load_ps1(b + 0)) };
    __m128 t1{ _mm_mul_ps(_mm_loadu_ps(a + 4), _mm_load_ps1(b + 1)) };
    __m128 t2{ _mm_mul_ps(_mm_loadu_ps(a + 8), _mm_load_ps1(b + 2)) };
    __m128 t3{ _mm_add_ps(t0, t1) };
    __m128 t4{ _mm_add_ps(t2, _mm_loadu_ps(a + 12)) };
    __m128 t5{ _mm_add_ps(t3, t4) };
    _mm_storeu_ps(out, t5);
#else
    out[0] = a[0] * b[0] + a[4] * b[1] + a[ 8] * b[2] + a[12];
    out[1] = a[1] * b[0] + a[5] * b[1] + a[ 9] * b[2] + a[13];
    out[2] = a[2] * b[0] + a[6] * b[1] + a[10] * b[2] + a[14];
    out[3] = a[3] * b[0] + a[7] * b[1] + a[11] * b[2] + a[15];
#endif
}

inline void mul4x4t_4(vec4 out, const mat4x4 a, const vec4 b)
{
#if defined(ARCH_INTEL) && defined(USE_SIMD)
    __m128 t0{ _mm_mul_ps(_mm_loadu_ps(a +  0), _mm_load_ps1(b + 0)) };
    __m128 t1{ _mm_mul_ps(_mm_loadu_ps(a +  4), _mm_load_ps1(b + 1)) };
    __m128 t2{ _mm_mul_ps(_mm_loadu_ps(a +  8), _mm_load_ps1(b + 2)) };
    __m128 t3{ _mm_mul_ps(_mm_loadu_ps(a + 12), _mm_load_ps1(b + 3)) };
    __m128 t4{ _mm_add_ps(t0, t1) };
    __m128 t5{ _mm_add_ps(t2, t3) };
    __m128 t6{ _mm_add_ps(t4, t5) };
    _mm_storeu_ps(out, t6);
#else
    out[0] = a[0] * b[0] + a[4] * b[1] + a[ 8] * b[2] + a[12] * b[3];
    out[1] = a[1] * b[0] + a[5] * b[1] + a[ 9] * b[2] + a[13] * b[3];
    out[2] = a[2] * b[0] + a[6] * b[1] + a[10] * b[2] + a[14] * b[3];
    out[3] = a[3] * b[0] + a[7] * b[1] + a[11] * b[2] + a[15] * b[3];
#endif
}

inline void mul4x4_4x4(mat4x4 out, const mat4x4 a, const mat4x4 b)
{
#if defined(ARCH_INTEL) && defined(USE_SIMD)
    __m128 t0, t1, t2, t3, t4, t5, t6;
    __m128 b0{ _mm_loadu_ps(b +  0) };
    __m128 b1{ _mm_loadu_ps(b +  4) };
    __m128 b2{ _mm_loadu_ps(b +  8) };
    __m128 b3{ _mm_loadu_ps(b + 12) };
    t0 = _mm_mul_ps(_mm_load_ps1(a +  0), b0);
    t1 = _mm_mul_ps(_mm_load_ps1(a +  1), b1);
    t2 = _mm_mul_ps(_mm_load_ps1(a +  2), b2);
    t3 = _mm_mul_ps(_mm_load_ps1(a +  3), b3);
    t4 = _mm_add_ps(t0, t1);
    t5 = _mm_add_ps(t2, t3);
    t6 = _mm_add_ps(t4, t5);
    _mm_storeu_ps(out +  0, t6);
    t0 = _mm_mul_ps(_mm_load_ps1(a +  4), b0);
    t1 = _mm_mul_ps(_mm_load_ps1(a +  5), b1);
    t2 = _mm_mul_ps(_mm_load_ps1(a +  6), b2);
    t3 = _mm_mul_ps(_mm_load_ps1(a +  7), b3);
    t4 = _mm_add_ps(t0, t1);
    t5 = _mm_add_ps(t2, t3);
    t6 = _mm_add_ps(t4, t5);
    _mm_storeu_ps(out +  4, t6);
    t0 = _mm_mul_ps(_mm_load_ps1(a +  9), b0);
    t1 = _mm_mul_ps(_mm_load_ps1(a +  8), b1);
    t2 = _mm_mul_ps(_mm_load_ps1(a + 10), b2);
    t3 = _mm_mul_ps(_mm_load_ps1(a + 11), b3);
    t4 = _mm_add_ps(t0, t1);
    t5 = _mm_add_ps(t2, t3);
    t6 = _mm_add_ps(t4, t5);
    _mm_storeu_ps(out +  8, t6);
    t0 = _mm_mul_ps(_mm_load_ps1(a + 12), b0);
    t1 = _mm_mul_ps(_mm_load_ps1(a + 13), b1);
    t2 = _mm_mul_ps(_mm_load_ps1(a + 14), b2);
    t3 = _mm_mul_ps(_mm_load_ps1(a + 15), b3);
    t4 = _mm_add_ps(t0, t1);
    t5 = _mm_add_ps(t2, t3);
    t6 = _mm_add_ps(t4, t5);
    _mm_storeu_ps(out + 12, t6);
#else
    out[ 0] = a[ 0] * b[ 0] + a[ 1] * b[ 4] + a[ 2] * b[ 8] + a[ 3] * b[12];
    out[ 1] = a[ 0] * b[ 1] + a[ 1] * b[ 5] + a[ 2] * b[ 9] + a[ 3] * b[13];
    out[ 2] = a[ 0] * b[ 2] + a[ 1] * b[ 6] + a[ 2] * b[10] + a[ 3] * b[14];
    out[ 3] = a[ 0] * b[ 3] + a[ 1] * b[ 7] + a[ 2] * b[11] + a[ 3] * b[15];
    out[ 4] = a[ 4] * b[ 0] + a[ 5] * b[ 4] + a[ 6] * b[ 8] + a[ 7] * b[12];
    out[ 5] = a[ 4] * b[ 1] + a[ 5] * b[ 5] + a[ 6] * b[ 9] + a[ 7] * b[13];
    out[ 6] = a[ 4] * b[ 2] + a[ 5] * b[ 6] + a[ 6] * b[10] + a[ 7] * b[14];
    out[ 7] = a[ 4] * b[ 3] + a[ 5] * b[ 7] + a[ 6] * b[11] + a[ 7] * b[15];
    out[ 8] = a[ 8] * b[ 0] + a[ 9] * b[ 4] + a[10] * b[ 8] + a[11] * b[12];
    out[ 9] = a[ 8] * b[ 1] + a[ 9] * b[ 5] + a[10] * b[ 9] + a[11] * b[13];
    out[10] = a[ 8] * b[ 2] + a[ 9] * b[ 6] + a[10] * b[10] + a[11] * b[14];
    out[11] = a[ 8] * b[ 3] + a[ 9] * b[ 7] + a[10] * b[11] + a[11] * b[15];
    out[12] = a[12] * b[ 0] + a[13] * b[ 4] + a[14] * b[ 8] + a[15] * b[12];
    out[13] = a[12] * b[ 1] + a[13] * b[ 5] + a[14] * b[ 9] + a[15] * b[13];
    out[14] = a[12] * b[ 2] + a[13] * b[ 6] + a[14] * b[10] + a[15] * b[14];
    out[15] = a[12] * b[ 3] + a[13] * b[ 7] + a[14] * b[11] + a[15] * b[15];
#endif
}

// transpose

inline void trn3x3(mat3x3 out, const mat3x3 in)
{
    out[0] = in[0];
    out[1] = in[3];
    out[2] = in[6];
    out[3] = in[1];
    out[4] = in[4];
    out[5] = in[7];
    out[6] = in[2];
    out[7] = in[5];
    out[8] = in[8];
}

inline void trn4x4(mat4x4 out, const mat4x4 in)
{
#if defined(ARCH_INTEL) && defined(USE_SIMD)
    __m128 a0{ _mm_loadu_ps(in +  0) };
    __m128 a1{ _mm_loadu_ps(in +  4) };
    __m128 a2{ _mm_loadu_ps(in +  8) };
    __m128 a3{ _mm_loadu_ps(in + 12) };
    __m128 t0{ _mm_unpacklo_ps(a0, a1) };
    __m128 t1{ _mm_unpacklo_ps(a2, a3) };
    __m128 t2{ _mm_unpackhi_ps(a0, a1) };
    __m128 t3{ _mm_unpackhi_ps(a2, a3) };
    _mm_storeu_ps(out +  0, _mm_movelh_ps(t0, t1));
    _mm_storeu_ps(out +  4, _mm_movehl_ps(t1, t0));
    _mm_storeu_ps(out +  8, _mm_movelh_ps(t2, t3));
    _mm_storeu_ps(out + 12, _mm_movehl_ps(t3, t2));
#else
    out[ 0] = in[ 0];
    out[ 1] = in[ 4];
    out[ 2] = in[ 8];
    out[ 3] = in[12];
    out[ 4] = in[ 1];
    out[ 5] = in[ 5];
    out[ 6] = in[ 9];
    out[ 7] = in[13];
    out[ 8] = in[ 2];
    out[ 9] = in[ 6];
    out[10] = in[10];
    out[11] = in[14];
    out[12] = in[ 3];
    out[13] = in[ 7];
    out[14] = in[11];
    out[15] = in[15];
#endif
}

// dot

inline float dot3(const vec3 a, const vec3 b)
{
    return a[0] * b[0] + a[1] * b[1] + a[2] * b[2];
}

// cross

inline float cross2(float ax, float ay, float bx, float by)
{
    return ax * by - ay * bx;
};

inline void cross3(vec3 out, const vec3 a, const vec3 b)
{
    out[0] = a[1] * b[2] - a[2] * b[1];
    out[1] = a[2] * b[0] - a[0] * b[2];
    out[2] = a[0] * b[1] - a[1] * b[0];
};

// utility matrices

inline void rotation_x(mat3x3 out, const float angle)
{
    float c{ std::cos(angle) };
    float s{ std::sin(angle) };
    copy3x3(out, mat3x3
        {
            1,  0,  0,
            0,  c, -s,
            0,  s,  c
        });
}

inline void rotation_y(mat3x3 out, const float angle)
{
    float c{ std::cos(angle) };
    float s{ std::sin(angle) };
    copy3x3(out, mat3x3
        {
             c,  0,  s,
             0,  1,  0,
            -s,  0,  c
        });
}

inline void rotation_z(mat3x3 out, const float angle)
{
    float c{ std::cos(angle) };
    float s{ std::sin(angle) };
    copy3x3(out, mat3x3
        {
            c, -s,  0,
            s,  c,  0,
            0,  0,  1
        });
}

} // namespace lib3d::math
