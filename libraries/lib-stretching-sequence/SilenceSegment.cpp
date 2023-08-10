/*  SPDX-License-Identifier: GPL-2.0-or-later */
/*!********************************************************************

  Audacity: A Digital Audio Editor

  SilenceSegment.cpp

  Matthieu Hodgkinson

**********************************************************************/
#include "SilenceSegment.h"

#include <algorithm>
#include <cassert>

SilenceSegment::SilenceSegment(
   int sampleRate, size_t numChannels, double startTime, sampleCount numSamples)
    : mSampleRate { sampleRate }
    , mNumChannels { numChannels }
    , mStartTime { startTime }
    , mEndTime { startTime + numSamples.as_double() / sampleRate }
    , mNumRemainingSamples { numSamples }
{
}

size_t
SilenceSegment::GetFloats(std::vector<float*>& buffers, size_t numSamples)
{
   assert(buffers.size() == mNumChannels);
   const size_t numSamplesToProduce =
      std::min<long long>(mNumRemainingSamples.as_long_long(), numSamples);
   for (auto i = 0u; i < mNumChannels; ++i)
   {
      auto buffer = buffers[i];
      std::fill(buffer, buffer + numSamplesToProduce, 0.f);
   }
   mNumRemainingSamples -= numSamplesToProduce;
   return numSamplesToProduce;
}

bool SilenceSegment::Empty() const
{
   return mNumRemainingSamples == 0u;
}

size_t SilenceSegment::GetWidth() const
{
   return mNumChannels;
}

double SilenceSegment::GetPlayStartTime() const
{
   return mStartTime;
}

double SilenceSegment::GetPlayEndTime() const
{
   return mEndTime;
}

sampleCount SilenceSegment::TimeToSamples(double t) const
{
   return sampleCount { t * mSampleRate + .5 };
}
