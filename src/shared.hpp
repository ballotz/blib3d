#pragma once
#include <cstdint>

#if defined(_MSC_VER)

#define no_inline __declspec(noinline)
#define force_inline __forceinline

#ifdef _M_IX86
#define ARCH_X86
#endif

#ifdef _M_X64
#define ARCH_X64
#endif

#if defined(ARCH_X86) | defined(ARCH_X64)
#define ARCH_INTEL
#endif

#else

#error unknown compiler

#endif
