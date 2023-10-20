#pragma once

class WaveClipBoundaryManagerOwner
{
public:
   virtual ~WaveClipBoundaryManagerOwner() = default;
   virtual void SetEnvelopeOffset(double offset) = 0;
   virtual void RescaleEnvelopeTimesBy(double ratio) = 0;
   virtual double GetStretchedSequenceSampleCount() const = 0;
   virtual double GetStretchFactor() const = 0;
};
