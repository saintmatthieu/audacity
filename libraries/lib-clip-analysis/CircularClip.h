#pragma once

#include "ClipInterfacePartialImpl.h"

namespace ClipAnalysis
{
class CircularClip : public ClipInterfacePartialImpl
{
public:
   CircularClip(const ClipInterface& clip, int fftSize, int overlap);

   sampleCount GetVisibleSampleCount() const override;

   AudioSegmentSampleView GetSampleView(
      size_t iChannel, sampleCount start, size_t length,
      bool mayThrow = true) const override;

private:
   const int mFftSize;
   const int mOverlap;
};
} // namespace ClipAnalysis
