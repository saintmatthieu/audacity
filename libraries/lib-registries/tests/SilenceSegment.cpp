#include "SilenceSegment.h"
#include "SilenceSegmentProcessor.h"

AudioSegmentProcessor& SilenceSegment::GetProcessor() const
{
   return SilenceSegmentProcessor::Get(*this);
}
