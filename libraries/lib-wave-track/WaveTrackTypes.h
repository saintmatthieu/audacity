#pragma once

#include "SampleCount.h"

struct StretchedWaveTrackPlayoutState
{
   size_t clipIndex = 0u;
};

class AudioSegment
{
public:
   virtual ~AudioSegment() = default;
   double t0 = 0.0;
   virtual size_t GetAudio(float* const* buffer, size_t bufferSize) const = 0;
};
