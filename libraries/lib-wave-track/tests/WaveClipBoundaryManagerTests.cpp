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

struct Expectations
{
   double sequenceOffset { 0 };
   sampleCount playStartSample { 0 };
   sampleCount playEndSample { 0 };
   double trimLeft { 0 };
   double trimRight { 0 };
   sampleCount numLeadingHiddenStems { 0 };
   sampleCount numTrailingHiddenStems { 0 };
};

void VerifyExpectations(
   const WaveClipBoundaryManager& sut, const Expectations& expected)
{
   REQUIRE(sut.GetSequenceOffset() == expected.sequenceOffset);
   REQUIRE(sut.GetPlayStartSample() == expected.playStartSample);
   REQUIRE(sut.GetPlayEndSample() == expected.playEndSample);
   REQUIRE(sut.GetTrimLeft() == expected.trimLeft);
   REQUIRE(sut.GetTrimRight() == expected.trimRight);
   REQUIRE(sut.GetNumLeadingHiddenStems() == expected.numLeadingHiddenStems);
   REQUIRE(sut.GetNumTrailingHiddenStems() == expected.numTrailingHiddenStems);
}

} // namespace

TEST_CASE("WaveClipBoundaryManager")
{
   SECTION("Boundary getters")
   {
      MockWaveClipBoundaryManagerOwner owner;
      WaveClipBoundaryManager sut { owner, sampleRate };
      owner.numRawSamples = 100;

      Expectations expectations;
      expectations.playEndSample = 100;
      VerifyExpectations(sut, expectations);

      owner.stretchFactor = 2;
      expectations.playEndSample = 200;

      sut.SetSequenceOffset(1);
      expectations.sequenceOffset = 1;
      expectations.playStartSample = 10;
      expectations.playEndSample = 210;

      sut.TrimLeftTo(sampleCount { 11 });
      sut.TrimRightTo(sampleCount { 209 });

      expectations.trimLeft = 1. / sampleRate;
      expectations.trimRight = 1. / sampleRate;
      expectations.playStartSample += 1;
      expectations.playEndSample -= 1;
      expectations.numLeadingHiddenStems =
         1; // Although half an unstretched sample period is hidden.
      expectations.numTrailingHiddenStems =
         0; // Halve an unstretched sample period.

      sut.TrimLeftTo(sampleCount { 12 });
      sut.TrimRightTo(sampleCount { 208 });
      expectations.trimLeft = 2. / sampleRate;
      expectations.trimRight = 2. / sampleRate;
      expectations.playStartSample += 1;
      expectations.playEndSample -= 1;
      expectations.numLeadingHiddenStems = 1;
      expectations.numTrailingHiddenStems = 1;
   }

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
