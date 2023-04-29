#ifndef __AUDACITY_TIME_AND_PITCH_INTERFACE__
#define __AUDACITY_TIME_AND_PITCH_INTERFACE__

#include <functional>
#include <memory>
#include <vector>

class TIME_AND_PITCH_API TimeAndPitchInterface
{
public:
   using InputGetter =
      std::function<void(float* const*, size_t samplesPerChannel)>;
   static std::unique_ptr<TimeAndPitchInterface>
   createInstance(size_t numChannels, InputGetter);

   virtual bool GetSamples(float* const*, size_t) = 0;

   virtual ~TimeAndPitchInterface() = default;
};

#endif // __AUDACITY_TIME_AND_PITCH_INTERFACE__