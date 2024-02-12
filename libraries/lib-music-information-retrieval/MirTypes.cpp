/*  SPDX-License-Identifier: GPL-2.0-or-later */
/*!********************************************************************

  Audacity: A Digital Audio Editor

  MirTypes.cpp

  Matthieu Hodgkinson

**********************************************************************/

#include "MirTypes.h"
#include <pffft.h>

namespace MIR
{
float* PffftFloatVectorAllocator::allocate(std::size_t n)
{
   return reinterpret_cast<float*>(pffft_aligned_malloc(n));
}

void PffftFloatVectorAllocator::deallocate(float* const p, std::size_t n)
{
   pffft_aligned_free(p);
}
} // namespace MIR
