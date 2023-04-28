#include "StaffPadTimeAndPitch.h"
#include <memory>

StaffPadTimeAndPitch::StaffPadTimeAndPitch()
{
}

const TimeAndPitchInterface::Factory TimeAndPitchInterface::factory =
   [](bool sampleRate, size_t numInstances)
   -> std::vector<std::shared_ptr<TimeAndPitchInterface>> {
   std::vector<std::shared_ptr<TimeAndPitchInterface>> stretchers(
      numInstances);
   for (auto& stretcher : stretchers)
   {
      // stretcher.reset(new StaffPadTimeAndPitch(sampleRate));
   }
   return stretchers;
};

bool StaffPadTimeAndPitch::GetSamples(float* const* output, size_t outputLen)
{
   return false;
}
