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

   void Process() override
   {
   }

   static SilenceSegmentProcessor& Get(const SilenceSegment&);

private:
   const SilenceSegment& mSilenceSegment;
};
