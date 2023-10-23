#include "ResamplingClip.h"

#include <array>

namespace ClipAnalysis
{
ResamplingClip::ResamplingClip(const ClipInterface& clip, double outSampleRate)
    : ClipInterfacePartialImpl { clip }
    , mResampleFactor { outSampleRate / clip.GetRate() }
    , mNumSamplesAfterResampling { static_cast<sampleCount>(
         clip.GetVisibleSampleCount().as_double() * mResampleFactor) }
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
   constexpr auto blockSize = 1024;
   auto numOutSamples = 0;
   std::array<float, blockSize> buffer;
   while (mOut.size() < length)
   {
      const auto sampleView =
         mClip.GetSampleView(iChannel, *mReadStart, blockSize, mayThrow);
      sampleView.Copy(buffer.data(), blockSize);
      constexpr auto isLast = false; // We're reading a loop - never last.
      const auto [consumed, produced] = mResampler.Process(
         mResampleFactor, buffer.data(), blockSize, isLast, buffer.data(),
         blockSize);
      std::copy(
         buffer.data(), buffer.data() + produced, std::back_inserter(mOut));
      mReadStart = *mReadStart + consumed;
   }
   auto block =
      std::make_shared<std::vector<float>>(mOut.begin(), mOut.begin() + length);
   mOut.erase(mOut.begin(), mOut.begin() + length);
   return { { block }, 0, length };
}
} // namespace ClipAnalysis
