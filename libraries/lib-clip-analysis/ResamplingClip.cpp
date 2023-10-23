#include "ResamplingClip.h"

#include <array>

namespace ClipAnalysis
{
ResamplingClip::ResamplingClip(const ClipInterface& clip, double outSampleRate)
    : ClipInterfacePartialImpl { clip }
    , mResampleFactor { outSampleRate / clip.GetRate() }
    , mNumSamplesAfterResampling { static_cast<sampleCount>(
         clip.GetVisibleSampleCount().as_double() * mResampleFactor + .5) }
    , mResampler { true, mResampleFactor, mResampleFactor }
{
}

sampleCount ResamplingClip::GetVisibleSampleCount() const
{
   return mNumSamplesAfterResampling;
}

AudioSegmentSampleView ResamplingClip::GetSampleView(
   size_t iChannel, sampleCount start, size_t length, bool mayThrow) const
{
   return const_cast<ResamplingClip*>(this)->MutableGetSampleView(
      iChannel, start, length, mayThrow);
}

AudioSegmentSampleView ResamplingClip::MutableGetSampleView(
   size_t iChannel, sampleCount start, size_t length, bool mayThrow)
{
   if (!mReadStart.has_value())
      mReadStart = sampleCount { start.as_double() / mResampleFactor };
   constexpr auto maxBlockSize = 1024;
   std::array<float, maxBlockSize> in, out;
   const auto maxNumSamples = mClip.GetVisibleSampleCount();
   while (mOut.size() < length)
   {
      const auto numToRead =
         std::min({ static_cast<size_t>(maxBlockSize),
                    (maxNumSamples - *mReadStart).as_size_t(), length });
      const auto sampleView =
         mClip.GetSampleView(iChannel, *mReadStart, numToRead, mayThrow);
      const auto isLast = *mReadStart + numToRead >= maxNumSamples;
      const auto numRead = sampleView.GetSampleCount();
      sampleView.Copy(in.data(), numRead);
      const auto [consumed, produced] = mResampler.Process(
         mResampleFactor, in.data(), numRead, isLast, out.data(), maxBlockSize);
      std::copy(out.data(), out.data() + produced, std::back_inserter(mOut));
      mReadStart = *mReadStart + consumed;
      if (isLast && produced == 0)
         break;
   }
   const auto outSize = std::min(mOut.size(), length);
   auto block = std::make_shared<std::vector<float>>(
      mOut.begin(), mOut.begin() + outSize);
   mOut.erase(mOut.begin(), mOut.begin() + outSize);
   return { { block }, 0, outSize };
}
} // namespace ClipAnalysis
