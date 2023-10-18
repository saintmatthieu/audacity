#include "WaveClipBoundaryManager.h"

#include "catch2/catch.hpp"

class MockWaveClipBoundaryManagerOwner : public WaveClipBoundaryManagerOwner
{
public:
   void SetEnvelopeOffset(double offset) override
   {
   }
   void RescaleEnvelopeTimesBy(double ratio) override
   {
   }
};

TEST_CASE("WaveClipBoundaryManager")
{
   MockWaveClipBoundaryManagerOwner owner;
   WaveClipBoundaryManager sut { owner, 100 };
   REQUIRE(sut.GetSequenceOffset() == 0);
   REQUIRE(sut.GetPlayStartSample() == 0);
   REQUIRE(sut.GetPlayEndSample() == 0);
   REQUIRE(sut.GetPlayStartTime() == 0);
   REQUIRE(sut.GetPlayEndTime() == 0);
   REQUIRE(sut.GetTrimLeft() == 0);
}
