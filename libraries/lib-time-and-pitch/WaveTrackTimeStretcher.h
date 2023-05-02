#pragma once // TODO(mhodgkinson) need ifndef ?

#include "TimeAndPitchInterface.h"
#include "WaveTrack.h"

#include <memory>

class TIME_AND_PITCH_API WaveTrackTimeStretcher final :
    public WaveTrackTimeStretcherBase
{
public:
   // TODO(mhodgkinson): why explicit ?
   explicit WaveTrackTimeStretcher(WaveTrack&);

   void GetFloats(float* const* buffers, size_t samplesPerChannel) override;

   // todo(mhodgkinson) are these needed ?
   static WaveTrackTimeStretcher& Get(WaveTrack&);
   static const WaveTrackTimeStretcher& Get(const WaveTrack&);

private:
   WaveTrack& mTrack;

   struct ClipReadoutState
   {
      ClipReadoutState(const WaveClipHolder& clip, sampleCount sampleIndex = 0)
          : clip(clip)
          , sampleIndex(sampleIndex)
      {
      }
      const WaveClipHolder clip;
      sampleCount sampleIndex;
   };
   std::unique_ptr<TimeAndPitchInterface> mStretcher;
   sampleCount mStretchedPlaySampleIndex = 0;
   std::unique_ptr<ClipReadoutState> mCurrentClipReadoutState;
};