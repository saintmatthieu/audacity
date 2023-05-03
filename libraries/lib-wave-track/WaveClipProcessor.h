#pragma once

#include "AudioSegment.h"
#include "WaveClip.h"

class WaveClipProcessor : public AudioSegmentProcessor
{
public:
   WaveClipProcessor(WaveClip& clip)
       : mClip(clip)
   {
   }

   size_t Process(
      float* const* buffer, size_t numChannels,
      size_t samplesPerChannel) override;

   static WaveClipProcessor& Get(const WaveClip& clip);

private:
   WaveClip& mClip;
};
