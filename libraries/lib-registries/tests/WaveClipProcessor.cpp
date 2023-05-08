#include "WaveClipProcessor.h"

static AudioSegment::Processor::RegisteredFactory sKeyS { [](AudioSegment& clip) {
   return std::make_unique<WaveClipProcessor>(static_cast<WaveClip&>(clip));
} };

WaveClipProcessor& WaveClipProcessor::Get(const WaveClip& clip)
{
   return const_cast<WaveClip&>(clip) // Consider it mutable data
      .Processor::Get<WaveClipProcessor>(sKeyS);
}