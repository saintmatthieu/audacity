#include "StaffPadTimeAndPitch.h"
#include <memory>

StaffPadTimeAndPitch::StaffPadTimeAndPitch(
   size_t numChannels, InputGetter inputGetter)
    : mInputGetter(inputGetter)
{
}

bool StaffPadTimeAndPitch::GetSamples(float* const* output, size_t outputLen)
{
   return false;
}
