#include "CircularClip.h"

#include <cassert>

namespace ClipAnalysis
{

CircularClip::CircularClip(const ClipInterface& clip, int fftSize, int overlap)
    : ClipInterfacePartialImpl { clip }
    , mFftSize { fftSize }
    , mOverlap { overlap }
{
}

sampleCount CircularClip::GetVisibleSampleCount() const
{
   return sampleCount {
      mClip.GetVisibleSampleCount().as_double() +
      static_cast<double>(mFftSize) * (mOverlap - 1) / mOverlap + 0.5
   };
}

AudioSegmentSampleView CircularClip::GetSampleView(
   size_t iChannel, sampleCount start, size_t length, bool mayThrow) const
{
   const auto numSamples = mClip.GetVisibleSampleCount();
   auto clipStart = start - mFftSize / 2;
   if (clipStart < 0)
      clipStart += numSamples;
   else if (clipStart >= numSamples)
      clipStart -= numSamples;
   auto clipEnd = std::min(numSamples, clipStart + length);
   auto firstView = mClip.GetSampleView(
      iChannel, clipStart, (clipEnd - clipStart).as_size_t(), mayThrow);
   if (clipEnd - clipStart == length)
      return firstView;
   auto secondView = mClip.GetSampleView(
      iChannel, 0, (length - (clipEnd - clipStart)).as_size_t(), mayThrow);
   // This is not very efficient, but it's just a prototype.
   auto newSamples = std::make_shared<std::vector<float>>();
   newSamples->resize(length);
   firstView.Copy(newSamples->data(), firstView.GetSampleCount());
   secondView.Copy(
      newSamples->data() + firstView.GetSampleCount(),
      secondView.GetSampleCount());
   return AudioSegmentSampleView { { newSamples }, 0, length };
}
} // namespace ClipAnalysis
