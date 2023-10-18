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

   double GetSequenceOffset() const;
   sampleCount GetPlayStartSample() const;
   sampleCount GetPlayEndSample() const;
   double GetTrimLeft() const;
   double GetTrimRight() const;
   sampleCount GetNumTrimmedSamplesLeft() const;
   sampleCount GetNumTrimmedSamplesRight() const;
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

   WaveClipBoundaryManagerOwner& mOwner;
   int mSampleRate;
   double mSequenceOffset { 0 };
   double mPlayStartTime { 0 };
   double mPlayEndTime { 0 };
};
