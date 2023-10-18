#pragma once

#include "SampleCount.h"

class XMLWriter;
class Envelope;

class WAVE_TRACK_API WaveClipBoundaryManager
{
public:
   WaveClipBoundaryManager(int sampleRate);
   WaveClipBoundaryManager(const WaveClipBoundaryManager& other);

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

   void SetSequenceOffset(double offset, Envelope& envelope);
   void DragPlayStartSampleTo(sampleCount sample, Envelope& envelope);
   void SetPlayEndSample(sampleCount sample);
   void OnProjectTempoChange(double newToOldRatio, Envelope& envelope);
   void StretchFromLeft(double newToOldRatio, Envelope& envelope);
   void StretchFromRight(double newToOldRatio, Envelope& envelope);
   void RescaleAround(double origin, double ratio, Envelope& envelope);
   void ChangeSampleRate(double newSampleRate, Envelope& envelope);
   void SetTrimLeft(double trim);
   void SetTrimRight(
      double trim, sampleCount sequenceSampleCount, double stretchRatio);

   void WriteXML(
      XMLWriter& xmlFile, sampleCount sequenceSampleCount,
      double stretchRatio) const;

private:
   void ShiftBy(sampleCount offset, Envelope& envelope);

   int mSampleRate;
   double mSequenceOffset { 0 };
   double mPlayStartTime { 0 };
   double mPlayEndTime { 0 };
};
