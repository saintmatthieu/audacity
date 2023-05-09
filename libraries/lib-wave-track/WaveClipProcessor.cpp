#include "WaveClipProcessor.h"
#include "Sequence.h"

#include <cassert>

static AudioSegment::Processor::RegisteredFactory sKeyS {
   [](AudioSegment& clip)
   { return std::make_unique<WaveClipProcessor>(static_cast<WaveClip&>(clip)); }
};

WaveClipProcessor& WaveClipProcessor::Get(const WaveClip& clip)
{
   return const_cast<WaveClip&>(clip) // Consider it mutable data
      .Processor::Get<WaveClipProcessor>(sKeyS);
}

WaveClipProcessor::WaveClipProcessor(const WaveClip& clip)
    : mClip(clip)
{
}

void WaveClipProcessor::Reposition(double t)
{
   const auto fitsClipPlayDuration =
      0.0 <= t && t < mClip.GetStretchedPlayDuration();
   assert(fitsClipPlayDuration);
   if (!fitsClipPlayDuration)
   {
      // TODO(mhodgkinson) better ?
      return;
   }
   mReadPos = static_cast<sampleCount::type>(
      t * mClip.GetRate() * mClip.GetTimeStretchRatio() + 0.5f);

   TimeAndPitchInterface::InputGetter inputGetter =
      [this, numChannels = 1u](float* const* buffers, size_t samplesPerChannel)
   {
      auto remaining = samplesPerChannel;
      while (remaining > 0)
      {
         const auto bestBlockSize =
            mClip.GetSequence()->GetBestBlockSize(mReadPos);
         const auto remainingSamplesInClip =
            mClip.GetPlaySamplesCount() - mReadPos;
         const auto numSamplesToWrite = std::min(bestBlockSize, remaining);
         const auto numSamplesToRead =
            std::min(sampleCount { numSamplesToWrite }, remainingSamplesInClip)
               .as_size_t();
         auto numZerosToPad = numSamplesToWrite - numSamplesToRead;
         constexpr auto mayThrow = false;
         const auto success = mClip.GetSamples(
            reinterpret_cast<char*>(buffers[0u]), floatSample, mReadPos,
            numSamplesToRead, mayThrow);
         assert(success);
         if (success)
         {
            mReadPos += numSamplesToRead;
         }
         else
         {
            numZerosToPad = numSamplesToWrite;
         }
         std::fill(buffers[0], buffers[0] + numZerosToPad, 0.f);
         remaining -= numSamplesToWrite;
      }
   };

   TimeAndPitchInterface::Parameters params;
   params.timeRatio = mClip.GetTimeStretchRatio();
   mStretcher = TimeAndPitchInterface::createInstance(
      1u, std::move(inputGetter), std::move(params));
}

size_t WaveClipProcessor::Process(
   float* const* buffer, size_t numChannels, size_t samplesPerChannel)
{
   assert(mStretcher);
   if (!mStretcher)
   {
      return 0u;
   }
   mStretcher->GetSamples(buffer, samplesPerChannel);
   return samplesPerChannel;
}

bool WaveClipProcessor::SamplesRemaining() const
{
   return mClip.GetPlaySamplesCount() - mReadPos > 0 || mStretcher;
}
