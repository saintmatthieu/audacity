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

   void Process() override
   {
      // Get data from clip, time-stretch, etc
   }

   static WaveClipProcessor& Get(const WaveClip& clip);

private:
   WaveClip& mClip;
};
