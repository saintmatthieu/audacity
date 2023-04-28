#include "TimeAndPitchInterface.h"

#include "StaffPad/TimeAndPitch.h"

class StaffPadTimeAndPitch final : public TimeAndPitchInterface
{
public:
   StaffPadTimeAndPitch();
   bool GetSamples(float* const*, size_t) override;

private:
   staffpad::TimeAndPitch mStretcher;
};