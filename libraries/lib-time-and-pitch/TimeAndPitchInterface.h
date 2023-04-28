#ifndef __AUDACITY_TIME_AND_PITCH__
#define __AUDACITY_TIME_AND_PITCH__

#include <functional>
#include <memory>
#include <vector>

class TIME_AND_PITCH_API TimeAndPitchInterface
{
public:
   using Factory =
      std::function<std::vector<std::shared_ptr<TimeAndPitchInterface>>(
         bool sampleRate, size_t numInstances)>;

   static const Factory factory;

   virtual bool GetSamples(float* const*, size_t) = 0;

   virtual ~TimeAndPitchInterface() = default;
};

#endif // __AUDACITY_TIME_AND_PITCH__