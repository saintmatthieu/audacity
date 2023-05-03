#include "SilenceSegmentProcessor.h"
#include "SilenceSegment.h"

static AudioSegment::Processor::RegisteredFactory sKeyS {
   [](AudioSegment& segment) {
      // todo(mhodgkinson) segment doesn't have to be mutable actually, but I
      // couldn't get it to compile with const
      return std::make_unique<SilenceSegmentProcessor>(
         static_cast<SilenceSegment&>(segment));
   }
};

SilenceSegmentProcessor&
SilenceSegmentProcessor::Get(const SilenceSegment& segment)
{
   return const_cast<SilenceSegment&>(segment)
      .Processor::Get<SilenceSegmentProcessor>(sKeyS);
}

size_t SilenceSegmentProcessor::Process(
   float* const* buffer, size_t numChannels, size_t samplesPerChannel)
{
   for (auto i = 0u; i < numChannels;++i){
      std::fill(buffer[i], buffer[i] + samplesPerChannel, 0.f);
   }
   return samplesPerChannel;
}
