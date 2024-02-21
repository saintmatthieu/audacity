#include "AudioContainer.h"
#include "FormantFilter.h"
#include "StaffPad/TimeAndPitch.h"
#include "TimeAndPitchInterface.h"
#include <mutex>

class TIME_AND_PITCH_API StaffPadTimeAndPitch final :
    public TimeAndPitchInterface
{
public:
   StaffPadTimeAndPitch(
      int sampleRate, size_t numChannels, TimeAndPitchSource&,
      const Parameters&);
   void GetSamples(float* const*, size_t) override;
   void OnCentShiftChange(int cents) override;

private:
   void BootStretcher();
   bool IllState() const;
   std::unique_ptr<staffpad::TimeAndPitch> mTimeAndPitch;
   TimeAndPitchSource& mAudioSource;
   AudioContainer mReadBuffer;
   const int mSampleRate;
   const size_t mNumChannels;
   const double mTimeRatio;
   double mPitchRatio;
   std::mutex mTimeAndPitchMutex;
   FormantFilter mFormantFilter;
};
