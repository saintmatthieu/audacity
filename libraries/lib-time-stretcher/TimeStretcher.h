#include "TimeStretcherInterface.h"

class TimeStretcherSource
{
public:
   virtual bool
   GetSamples(float* const* buffer, size_t samplesPerChannel) const = 0;
   virtual ~TimeStretcherSource() = default;
};

class TimeStretcher : public TimeStretcherInterface
{
public:
   TimeStretcher(const TimeStretcherSource&, double sampleRate);
   bool GetSamples(float* const*, size_t) override;

private:
   const TimeStretcherSource& _source;
};