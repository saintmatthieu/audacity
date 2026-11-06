/*
 * Audacity: A Digital Audio Editor
 */
#pragma once

// MSVC has __has_builtin but not __builtin_readcyclecounter, so tracktion_CPU.h calls __rdtsc, which MSVC ARM64 lacks.
// Read the virtual counter instead, as tracktion does with other ARM compilers.

#include <intrin.h>

inline unsigned __int64 __rdtsc()
{
    // CNTVCT_EL0, as winnt.h's ARM64_CNTVCT, which <intrin.h> doesn't define: ARM64_SYSREG(3, 3, 14, 0, 2)
    constexpr int cntvct = 0x5F02;
    return _ReadStatusReg(cntvct);
}
