/*  SPDX-License-Identifier: GPL-2.0-or-later */
/*!********************************************************************

  Audacity: A Digital Audio Editor

  SilenceSegmentTest.cpp

  Matthieu Hodgkinson

**********************************************************************/
#include "SilenceSegment.h"
#include "AudioContainer.h"

#include <catch2/catch.hpp>

TEST_CASE("SilenceSegment", "behaves as expected")
{
   // A quick test of an easy implementation.
   const auto numChannels = GENERATE(1, 2);
   constexpr auto silenceSegmentLength = 3u;
   constexpr auto sampleRate = 10;
   const auto startTime = 0.0;
   SilenceSegment sut(sampleRate, numChannels, startTime, silenceSegmentLength);
   REQUIRE(!sut.Empty());
   AudioContainer container(3u, numChannels);
   REQUIRE(sut.GetFloats(container.channelPointers, 1) == 1);
   REQUIRE(!sut.Empty());
   REQUIRE(sut.GetFloats(container.channelPointers, 3) == 2);
   REQUIRE(sut.Empty());
}
