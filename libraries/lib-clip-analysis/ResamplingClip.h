#pragma once

#include "ClipInterfacePartialImpl.h"

#include "Resample.h"

#include <optional>

namespace ClipAnalysis
{

class ResamplingClip : public ClipInterfacePartialImpl
{
public:
   ResamplingClip(const ClipInterface& clip, double outSampleRate);

   sampleCount GetVisibleSampleCount() const;

   AudioSegmentSampleView GetSampleView(
      size_t iChannel, sampleCount start, size_t length,
      bool mayThrow = true) const override;

private:
   AudioSegmentSampleView MutableGetSampleView(
      size_t iChannel, sampleCount start, size_t length, bool mayThrow);

   const double mResampleFactor;
   const sampleCount mNumSamplesAfterResampling;
   Resample mResampler;
   std::optional<sampleCount> mReadStart;
   std::vector<float> mOut;
};
} // namespace ClipAnalysis
