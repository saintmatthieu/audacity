#include "TimeAndPitchInterface.h"

#include "StaffPadTimeAndPitch.h"

std::unique_ptr<TimeAndPitchInterface> TimeAndPitchInterface::createInstance(
   size_t numChannels, InputGetter inputGetter, Parameters parameters)
{
   return std::make_unique<StaffPadTimeAndPitch>(
      numChannels, std::move(inputGetter), std::move(parameters));
}
