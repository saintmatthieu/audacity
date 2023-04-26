#ifndef __AUDACITY_TIMESTRETCHER__
#define __AUDACITY_TIMESTRETCHER__

#include <functional>
#include <memory>
#include <vector>

class TimeStretcherInterface
{
public:
   using Factory =
      std::function<std::vector<std::shared_ptr<TimeStretcherInterface>>(
         bool sampleRate, size_t numInstances)>;

   static const Factory factory;

   virtual bool GetSamples(float* const*, size_t) = 0;

   virtual ~TimeStretcherInterface() = default;
};

#endif // __AUDACITY_TIMESTRETCHER__