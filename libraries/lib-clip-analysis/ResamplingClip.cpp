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
   size_t numOutSamples = 0;
   std::array<float, blockSize> in, out;
   const auto maxNumSamples = mClip.GetVisibleSampleCount();
   while (mOut.size() < length)
   {
      const auto numToRead =
         std::min<size_t>(blockSize, (maxNumSamples - *mReadStart).as_size_t());
      const auto sampleView =
         mClip.GetSampleView(iChannel, *mReadStart, numToRead, mayThrow);
      const auto isLast = *mReadStart + numToRead >= maxNumSamples;
      sampleView.Copy(in.data(), numToRead);
      const auto [consumed, produced] = mResampler.Process(
         mResampleFactor, in.data(), numToRead, isLast, out.data(), blockSize);
      std::copy(out.data(), out.data() + produced, std::back_inserter(mOut));
      mReadStart = *mReadStart + consumed;
      numOutSamples += produced;
      if (isLast && produced == 0)
         break;
   }
   auto block = std::make_shared<std::vector<float>>(
      mOut.begin(), mOut.begin() + numOutSamples);
   mOut.erase(mOut.begin(), mOut.begin() + numOutSamples);
   return { { block }, 0, numOutSamples };
}
} // namespace ClipAnalysis
