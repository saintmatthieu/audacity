#include "CircularClip.h"

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
   // We assume the clip to be a loop. We have to begin by giving the
   // SpectrumTransformer the last fftSize/2 samples of the clip, and at the
   // end again the first fftSize*(overlap - 1)/overlap samples.
   return mClip.GetVisibleSampleCount() + mFftSize / 2 +
          mFftSize * (mOverlap - 1);
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
