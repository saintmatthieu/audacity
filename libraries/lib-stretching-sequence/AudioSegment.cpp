#include "AudioSegment.h"

AudioSegment::~AudioSegment() = default;

sampleCount AudioSegment::GetNumOutputSamplesUpTo(double t /*absolute*/) const
{
   if (GetPlayStartTime() > t)
      return { 0 };
   t = std::min(GetPlayEndTime(), t);
   return TimeToSamples(t - GetPlayStartTime());
}
