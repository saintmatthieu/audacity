#pragma once

#include "AudioSegment.h"
#include "TimeAndPitchInterface.h"
#include "WaveClip.h"

class WaveClipProcessor : public AudioSegmentProcessor
{
public:
   WaveClipProcessor(const WaveClip& clip);
   void Reposition(double t) override;
   size_t Process(
      float* const* buffer, size_t numChannels,
      size_t samplesPerChannel) override;

   static WaveClipProcessor& Get(const WaveClip& clip);

private:
   const WaveClip& mClip;
   sampleCount mReadPos = 0;
   std::unique_ptr<TimeAndPitchInterface> mStretcher;
};
