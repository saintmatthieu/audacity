#pragma once

#include "SampleCount.h"

class WaveClipBoundaryManagerOwner
{
public:
   virtual ~WaveClipBoundaryManagerOwner() = default;
   virtual void SetEnvelopeOffset(double offset) = 0;
   virtual void RescaleEnvelopeTimesBy(double ratio) = 0;
   virtual sampleCount GetSequenceSampleCount() const = 0;
   virtual double GetStretchFactor() const = 0;
};
