#include "AudioContainer.h"
#include "FormantShifter.h"
#include "FormantShifterLogger.h"
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
   void OnFormantPreservationChange(bool preserve) override;

private:
   void BootStretcher();
   bool IllState() const;
   void ResetStretcher();

   const int mSampleRate;
   TimeAndPitchInterface::Parameters mParameters;
   FormantShifterLogger mFormantShifterLogger;
   FormantShifter mFormantShifter;
   std::unique_ptr<staffpad::TimeAndPitch> mTimeAndPitch;
   TimeAndPitchSource& mAudioSource;
   AudioContainer mReadBuffer;
   const size_t mNumChannels;
   std::mutex mTimeAndPitchMutex;
};
