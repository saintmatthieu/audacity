#include "WaveClipProcessor.h"

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

size_t WaveClipProcessor::Process(
   float* const* buffer, size_t numChannels, size_t samplesPerChannel)
{
   for (auto i = 0u; i < numChannels; ++i)
   {
      std::fill(buffer[i], buffer[i] + samplesPerChannel, 0.f);
   }
   return samplesPerChannel;
}
