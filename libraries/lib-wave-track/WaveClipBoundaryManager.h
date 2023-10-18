#pragma once

#include "SampleCount.h"

class XMLWriter;
class Envelope;

class WaveClipBoundaryManagerOwner
{
public:
   virtual ~WaveClipBoundaryManagerOwner() = default;
   virtual void SetEnvelopeOffset(double offset) = 0;
   virtual void RescaleEnvelopeTimesBy(double ratio) = 0;
};

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
   double GetPlayStartTime() const;
   double GetPlayEndTime() const;
   double GetTrimLeft() const;
   double
   GetTrimRight(sampleCount sequenceSampleCount, double stretchRatio) const;
   sampleCount GetNumTrimmedSamplesLeft() const;
   sampleCount GetNumTrimmedSamplesRight(
      sampleCount sequenceSampleCount, double stretchRatio) const;
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
   void SetTrimRight(
      double trim, sampleCount sequenceSampleCount, double stretchRatio);

   void WriteXML(
      XMLWriter& xmlFile, sampleCount sequenceSampleCount,
      double stretchRatio) const;

private:
   void ShiftBy(sampleCount offset);

   WaveClipBoundaryManagerOwner& mOwner;
   int mSampleRate;
   double mSequenceOffset { 0 };
   double mPlayStartTime { 0 };
   double mPlayEndTime { 0 };
};
