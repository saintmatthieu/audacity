#include "TimeAndPitchInterface.h"

#include "StaffPad/TimeAndPitch.h"

class StaffPadTimeAndPitch final : public TimeAndPitchInterface
{
public:
   StaffPadTimeAndPitch(size_t numChannels, InputGetter);
   bool GetSamples(float* const*, size_t) override;

private:
   staffpad::TimeAndPitch mStretcher;
   const InputGetter mInputGetter;
};