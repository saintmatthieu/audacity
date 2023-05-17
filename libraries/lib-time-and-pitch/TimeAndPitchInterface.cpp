#include "TimeAndPitchInterface.h"

#include "StaffPadTimeAndPitch.h"

std::unique_ptr<TimeAndPitchInterface> TimeAndPitchInterface::createInstance(
   size_t numChannels, AudioSource audioSource, Parameters parameters)
{
   return std::make_unique<StaffPadTimeAndPitch>(
      numChannels, std::move(audioSource), std::move(parameters));
}
