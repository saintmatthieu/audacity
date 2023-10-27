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
   void SetSequenceStartTime(double t);
   double GetSequenceEndTime() const;
   double GetSequenceDuration() const;
   double GetPlayStartTime() const;
   double GetPlayEndTime() const;
   double GetTrimLeft() const;
   void SetTrimLeft(double trim);
   void TrimLeft(double deltaTime);
   sampleCount GetNumStems() const;

   sampleCount TimeToSamples(double t) const;
   double SamplesToTime(sampleCount s) const;
   double SnapToTrackSample(double t) const;

   int GetRate() const;
   //! Also changes boundaries.
   void SetRate(int rate);
   //! Only changes the rate ; use with care.
   void HardsetRate(int rate);

   void
   OnProjectTempoChange(const std::optional<double>& oldTempo, double newTempo);
   void StretchLeftTo(double t);
   void StretchRightTo(double t);

   void RescaleTimesBy(double ratio);
   void TrimLeftTo(double t);

   //! Real-time durations, i.e., stretching the clip modifies these.
   //! @{
   double mTrimRight { 0 };
   //! @}

   // Used in GetStretchRatio which computes the factor, by which the sample
   // interval is multiplied, to get a realtime duration.
   double mClipStretchRatio = 1.;
   std::optional<double> mRawAudioTempo;
   std::optional<double> mProjectTempo;

protected:
   // Not size_t!  May need to be large:
   sampleCount mNumSamples { 0 };
   size_t mAppendBufferLen { 0 };

private:
   //! Real-time durations, i.e., stretching the clip modifies these.
   //! @{
   double mSequenceOffset { 0 };
   double mTrimLeft { 0 };
   //! @}

   //! Sample rate of the raw audio, i.e., before stretching.
   int mRate;
};
