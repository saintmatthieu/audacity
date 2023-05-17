#ifndef __AUDACITY_TIME_AND_PITCH_INTERFACE__
#define __AUDACITY_TIME_AND_PITCH_INTERFACE__

#include <functional>
#include <memory>
#include <optional>
#include <vector>

class TIME_AND_PITCH_API TimeAndPitchInterface
{
public:
   using AudioSource =
      std::function<void(float* const*, size_t samplesPerChannel)>;

   struct Parameters
   {
      std::optional<double> timeRatio;
      std::optional<double> pitchRatio;
   };

   static std::unique_ptr<TimeAndPitchInterface>
   createInstance(size_t numChannels, AudioSource, Parameters = {});

   // May re-initialize underlying implementation.
   virtual void SetParameters(double timeRatio, double pitchRatio) = 0;

   // Duration gets multiplied by that ratio.
   // May re-initialize underlying implementation. If you intend to modify both
   // time and pitch ratio at once, prefer SetParameters;
   virtual void SetTimeRatio(double) = 0;

   // Pitch gets multiplied by that ratio.
   // May re-initialize underlying implementation. If you intend to modify both
   // time and pitch ratio at once, prefer SetParameters;
   virtual void SetPitchRatio(double) = 0;

   virtual void GetSamples(float* const*, size_t) = 0;

   virtual ~TimeAndPitchInterface() = default;
};

#endif // __AUDACITY_TIME_AND_PITCH_INTERFACE__
