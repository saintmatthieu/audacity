/*!********************************************************************

Audacity: A Digital Audio Editor

TrackSpectrumTransformer.h

Paul Licameli

**********************************************************************/

#ifndef __AUDACITY_TRACK_SPECTRUM_TRANSFORMER__
#define __AUDACITY_TRACK_SPECTRUM_TRANSFORMER__

#include <cassert>
#include <functional>
#include <memory>
#include <vector>

#include "FFT.h"
#include "RealFFTf.h"
#include "SampleCount.h"
#include "SpectrumTransformer.h"

class WaveChannel;
class WaveTrack;

//! Subclass of SpectrumTransformer that rewrites a track
class WAVE_TRACK_API TrackSpectrumTransformer /* not final */ :
    public SpectrumTransformer
{
public:
   /*!
    @copydoc SpectrumTransformer::SpectrumTransformer(bool,
       eWindowFunctions, eWindowFunctions, size_t, unsigned, bool, bool)
    @pre `!needsOutput || pOutputTrack != nullptr`
    */
   TrackSpectrumTransformer(WaveChannel *pOutputTrack,
      bool needsOutput, eWindowFunctions inWindowType,
      eWindowFunctions outWindowType, size_t windowSize,
      unsigned stepsPerWindow, bool leadingPadding, bool trailingPadding
   )  : SpectrumTransformer{ needsOutput, inWindowType, outWindowType,
         windowSize, stepsPerWindow, leadingPadding, trailingPadding
      }
      , mOutputTrack{ pOutputTrack }
   {
      assert(!needsOutput || pOutputTrack != nullptr);
   }
   ~TrackSpectrumTransformer() override;

   //! Invokes Start(), ProcessSamples(), and Finish()
   bool Process(const WindowProcessor &processor, const WaveChannel &channel,
      size_t queueLength, sampleCount start, sampleCount len);

   //! Final flush and trimming of tail samples
   /*!
    @pre `outputTrack.IsLeader()`
    */
   static bool PostProcess(WaveTrack &outputTrack, sampleCount len);

protected:
   bool DoStart() override;
   void DoOutput(const float *outBuffer, size_t mStepSize) override;
   bool DoFinish() override;

private:
   WaveChannel *const mOutputTrack;
   const WaveChannel *mpChannel = nullptr;
};

#endif
