/* Copyright 2019 Alessio Ballotti <alessioballotti@tiscali.it> */

#pragma once
#include <cstdint>

// architecture

#ifdef _M_IX86
#define ARCH_X86
#endif

#ifdef _M_X64
#define ARCH_X64
#endif

#if defined(ARCH_X86) | defined(ARCH_X64)
#define ARCH_INTEL
#endif

// compiler

#if defined(_MSC_VER)

#define no_inline __declspec(noinline)
#define force_inline __forceinline

#elif defined(__GNUC__)

#define no_inline __attribute__((noinline))
#define force_inline __attribute__((always_inline))

#else

#error unknown compiler

#endif

// debug

#if defined(NDEBUG) && defined(DEBUG)
#error both NDEBUG and DEBUG defined
#endif

#if !defined(NDEBUG) && !defined(DEBUG)
#error neither NDEBUG or DEBUG defined
#endif

#if !defined(NDEBUG) && defined(DEBUG)
#define debug_mode
#endif

// config

#define USE_SIMD

// common

template<typename T, uint32_t size>
uint32_t array_size(T(&)[size])
{
    return size;
}
