#include "WaveClipProcessor.h"
#include "Sequence.h"

#include <cassert>

static AudioSegment::Processor::RegisteredFactory sKeyS {
   [](AudioSegment& clip) {
      return std::make_unique<WaveClipProcessor>(static_cast<WaveClip&>(clip));
   }
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
      [this,
       numChannels = 1u](float* const* buffers, size_t samplesPerChannel) {
         auto remaining = static_cast<sampleCount>(samplesPerChannel);
         while (remaining > 0)
         {
            const auto bestBlockSize = static_cast<sampleCount>(
               mClip.GetSequence()->GetBestBlockSize(mReadPos));
            const auto remainingSamplesInClip =
               mClip.GetPlaySamplesCount() - mReadPos;
            const auto numSamplesToRead =
               std::min({ bestBlockSize, remainingSamplesInClip, remaining })
                  .as_size_t();
            constexpr auto mayThrow = false;
            const auto success = mClip.GetSamples(
               reinterpret_cast<char*>(buffers[0u]), floatSample, mReadPos,
               numSamplesToRead, mayThrow);
            assert(success);
            if (!success)
            {
               std::fill(buffers[0], buffers[0] + numSamplesToRead, 0.f);
            }
            remaining -= numSamplesToRead;
         }
      };

   mStretcher =
      TimeAndPitchInterface::createInstance(1u, std::move(inputGetter));
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
