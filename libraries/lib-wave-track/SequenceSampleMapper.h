#pragma once

#include "SampleCount.h"

#include <optional>

class WAVE_TRACK_API SequenceSampleMapper
{
public:
   SequenceSampleMapper(int rate);
   SequenceSampleMapper(const SequenceSampleMapper& orig);
   SequenceSampleMapper(const SequenceSampleMapper& orig, double t0, double t1);

   double GetStretchRatio() const;
   double GetSequenceStartTime() const;
   double GetSequenceEndTime() const;
   double GetPlayStartTime() const;
   double GetPlayEndTime() const;

   sampleCount TimeToSamples(double t) const;
   double SamplesToTime(sampleCount s) const;
   double SnapToTrackSample(double t) const;

   //! Real-time durations, i.e., stretching the clip modifies these.
   //! @{
   double mSequenceOffset { 0 };
   double mTrimLeft { 0 };
   double mTrimRight { 0 };
   //! @}

   // Used in GetStretchRatio which computes the factor, by which the sample
   // interval is multiplied, to get a realtime duration.
   double mClipStretchRatio = 1.;
   std::optional<double> mRawAudioTempo;
   std::optional<double> mProjectTempo;

   //! Sample rate of the raw audio, i.e., before stretching.
   int mRate;

protected:
   // Not size_t!  May need to be large:
   sampleCount mNumSamples { 0 };
   size_t mAppendBufferLen { 0 };
};
