#pragma once

#include "SampleCount.h"

class XMLWriter;
class WaveClipBoundaryManagerOwner;

class WAVE_TRACK_API WaveClipBoundaryManager
{
public:
   WaveClipBoundaryManager(WaveClipBoundaryManagerOwner& owner, int sampleRate);
   WaveClipBoundaryManager(
      WaveClipBoundaryManagerOwner& owner,
      const WaveClipBoundaryManager& other);

   // Number of stems, or visible samples, accounting from trimming and
   // stretching.
   sampleCount GetNumStems() const;
   double GetStemTime(sampleCount stemIndex) const;

   double GetSequenceOffset() const;
   sampleCount GetPlayStartSample() const;
   sampleCount GetPlayEndSample() const;
   double GetTrimLeft() const;
   double GetTrimRight() const;
   sampleCount GetNumLeadingHiddenStems() const;
   sampleCount GetNumTrailingHiddenStems() const;
   double GetRatioChangeWhenStretchingLeftTo(double to) const;
   double GetRatioChangeWhenStretchingRightTo(double to) const;

   void SetSequenceOffset(double offset);
   void DragPlayStartSampleTo(sampleCount sample);
   void SetPlayEndSample(sampleCount sample);
   void OnProjectTempoChange(double newToOldRatio);
   void StretchFromLeft(double newToOldRatio);
   void StretchFromRight(double newToOldRatio);
   void RescaleAround(double origin, double ratio);
   void ChangeSampleRate(double newSampleRate);
   void SetTrimLeft(double trim);
   void SetTrimRight(double trim);

   void WriteXML(XMLWriter& xmlFile) const;

private:
   void ShiftBy(sampleCount offset);
   double GetPlayStartTime() const;
   double GetPlayEndTime() const;
   sampleCount GetFirstStemIndex(double stretchRatio) const;
   sampleCount GetLastStemIndex(double stretchRatio) const;

   WaveClipBoundaryManagerOwner& mOwner;
   int mSampleRate;
   double mSequenceOffset { 0 };
   double mTrimLeft { 0 };
   double mTrimRight { 0 };
};
