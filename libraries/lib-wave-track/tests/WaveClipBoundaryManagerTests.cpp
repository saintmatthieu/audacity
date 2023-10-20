#include "WaveClipBoundaryManager.h"
#include "WaveClipBoundaryManagerOwner.h"

#include <catch2/catch.hpp>

struct MockWaveClipBoundaryManagerOwner : public WaveClipBoundaryManagerOwner
{
   sampleCount numRawSamples { 0 };
   double stretchFactor { 1 };

   void SetEnvelopeOffset(double offset) override
   {
   }

   void RescaleEnvelopeTimesBy(double ratio) override
   {
   }

   double GetStretchedSequenceSampleCount() const override
   {
      return numRawSamples.as_double() * stretchFactor;
   }
};

TEST_CASE("WaveClipBoundaryManager")
{
   SECTION("First and last lollypops always visible when no trim")
   {
      constexpr auto sampleRate = 10;
      MockWaveClipBoundaryManagerOwner owner;
      WaveClipBoundaryManager sut(owner, sampleRate);

   }
}
