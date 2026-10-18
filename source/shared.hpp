/* Copyright 2019 Alessio Ballotti <alessioballotti@tiscali.it> */

#pragma once
#include <cstdint>

// architecture

#if defined(_M_IX86) || defined(__i386__)
#define BLIB3D_ARCH_X86
#endif

#if defined(_M_X64) || defined(__x86_64__)
#define BLIB3D_ARCH_X64
#endif

#if defined(BLIB3D_ARCH_X86) || defined(BLIB3D_ARCH_X64)
#define BLIB3D_ARCH_INTEL
#endif

#if defined(BLIB3D_ARCH_X64) || (defined(_M_IX86_FP) && _M_IX86_FP >= 2) || defined(__SSE2__)
#define BLIB3D_ARCH_SSE
#endif

#if defined(_M_ARM) || defined(__arm__)
#define BLIB3D_ARCH_ARM32
#endif

#if defined(_M_ARM64) || defined(__aarch64__)
#define BLIB3D_ARCH_ARM64
#endif

#if defined(BLIB3D_ARCH_ARM32) || defined(BLIB3D_ARCH_ARM64)
#define BLIB3D_ARCH_ARM
#endif

#if defined(BLIB3D_ARCH_ARM32) && defined(__ARM_FEATURE_DSP)
#define BLIB3D_ARM_DSP
#endif

#if defined(BLIB3D_ARCH_ARM) && defined(__ARM_NEON)
#define BLIB3D_ARM_NEON
#endif

// compiler

#if defined(_MSC_VER)

#define blib3d_no_inline __declspec(noinline)
#define blib3d_force_inline __forceinline

#elif defined(__GNUC__)

#define blib3d_no_inline __attribute__((noinline))
#define blib3d_force_inline __attribute__((always_inline)) inline

#else

#error unknown compiler

#endif

// debug

#if !defined(NDEBUG)
#define blib3d_debug_mode
#endif

// common

template<typename T, uint32_t size>
uint32_t array_size(T(&)[size])
{
    return size;
}
