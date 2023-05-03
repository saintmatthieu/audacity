#pragma once

#include "AudioSegment.h"

class SilenceSegment;

class SilenceSegmentProcessor : public AudioSegmentProcessor
{
public:
   SilenceSegmentProcessor(const SilenceSegment& segment)
       : mSilenceSegment(segment)
   {
   }

   size_t Process(
      float* const* buffer, size_t numChannels,
      size_t samplesPerChannel) override;

   static SilenceSegmentProcessor& Get(const SilenceSegment&);

private:
   const SilenceSegment& mSilenceSegment;
};
