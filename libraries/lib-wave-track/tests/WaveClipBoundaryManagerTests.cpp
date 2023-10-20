#include "WaveClipBoundaryManager.h"
#include "WaveClipBoundaryManagerOwner.h"

#include <catch2/catch.hpp>

namespace
{
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

   sampleCount GetSequenceSampleCount() const override
   {
      return numRawSamples;
   }

   double GetStretchFactor() const override
   {
      return stretchFactor;
   }
};

constexpr auto sampleRate = 10;
} // namespace

TEST_CASE("WaveClipBoundaryManager")
{
   SECTION("SetPlayStartSample")
   {
      MockWaveClipBoundaryManagerOwner owner;
      WaveClipBoundaryManager sut { owner, sampleRate };
      owner.numRawSamples = 100;
      owner.stretchFactor = std::sqrt(2.0);

      constexpr auto offset = 1.234;
      sut.SetSequenceOffset(offset);

      const auto originalPlayStartSample = sut.GetPlayStartSample();
      const auto originalPlayEndSample = sut.GetPlayEndSample();

      // sut.SetPlayStartSample(10);
      REQUIRE(sut.GetPlayStartSample() == originalPlayStartSample + 10);
      REQUIRE(sut.GetPlayEndSample() == originalPlayEndSample + 10);
   }
}
