#include "WaveClipBoundaryManager.h"

#include "catch2/catch.hpp"

TEST_CASE("WaveClipBoundaryManager")
{
   WaveClipBoundaryManager sut { 100 };
   REQUIRE(sut.GetSequenceOffset() == 0);
   REQUIRE(sut.GetPlayStartSample() == 0);
   REQUIRE(sut.GetPlayEndSample() == 0);
   REQUIRE(sut.GetPlayStartTime() == 0);
   REQUIRE(sut.GetPlayEndTime() == 0);
   REQUIRE(sut.GetTrimLeft() == 0);
}
