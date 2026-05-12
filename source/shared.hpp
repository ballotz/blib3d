/* Copyright 2019 Alessio Ballotti <alessioballotti@tiscali.it> */

#pragma once
#include <cstdint>

// architecture

#ifdef _M_IX86
#define BLIB3D_ARCH_X86
#endif

#ifdef _M_X64
#define BLIB3D_ARCH_X64
#endif

#if defined(BLIB3D_ARCH_X86) | defined(BLIB3D_ARCH_X64)
#define BLIB3D_ARCH_INTEL
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
