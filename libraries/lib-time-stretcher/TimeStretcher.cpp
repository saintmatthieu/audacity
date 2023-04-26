#include "TimeStretcher.h"
#include <memory>

TimeStretcher::TimeStretcher(
   const TimeStretcherSource& source, double sampleRate)
    : _source(source)
{
}

const TimeStretcherInterface::Factory TimeStretcherInterface::factory =
   [](bool sampleRate, size_t numInstances)
   -> std::vector<std::shared_ptr<TimeStretcherInterface>>
{
   std::vector<std::shared_ptr<TimeStretcherInterface>> stretchers(
      numInstances);
   for (auto& stretcher : stretchers)
   {
      // stretcher.reset(new TimeStretcher(sampleRate));
   }
   return stretchers;
};

bool TimeStretcher::GetSamples(float* const* output, size_t outputLen)
{
   return false;
}
