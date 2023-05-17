#include "TimeAndPitchInterface.h"

#include "StaffPad/TimeAndPitch.h"

class TIME_AND_PITCH_API StaffPadTimeAndPitch final :
    public TimeAndPitchInterface
{
public:
   StaffPadTimeAndPitch(size_t numChannels, AudioSource, Parameters);
   void SetParameters(double timeRatio, double pitchRatio) override;
   void SetTimeRatio(double) override;
   void SetPitchRatio(double) override;
   void GetSamples(float* const*, size_t) override;

private:
   bool _SetRatio(double r, double& member, bool rebootOnSuccess);
   void _BootStretcher();
   std::unique_ptr<staffpad::TimeAndPitch> mTimeAndPitch;
   const AudioSource mAudioSource;
   const size_t mNumChannels;
   double mTimeRatio = 1.0;
   double mPitchRatio = 1.0;
};
