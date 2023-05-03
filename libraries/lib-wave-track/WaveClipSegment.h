#pragma once

#include "WaveClip.h"

class WaveClipSegment : public WaveClipProcessor, public AudioSegment
{
public:
   static WaveClipSegment& Get(const WaveClip&);

   void SetReadPosition(double t) override;
   size_t GetAudio(float* const* buffer, size_t bufferSize) const override;
   size_t Process(float* const* buffer, size_t bufferSize) override
   {
      return 0;
   }

private:
};
