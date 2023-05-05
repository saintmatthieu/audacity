#include "StaffPadTimeAndPitch.h"
#include "AudioContainer.h"

#include <catch2/catch.hpp>
#include <iostream>

TEST_CASE("StaffPadTimeAndPitch")
{
   SECTION("stuff")
   {
      constexpr auto numChannels = 1u;
      constexpr auto clipLength = 100u;

      // Simulate an audio source that has `clipLength` number of samples.
      auto numPulledInputSamples = 0u;
      TimeAndPitchInterface::InputGetter dcOffsetGetter {
         [&](float* const* buffer, size_t samplesPerChannel) {
            const auto numNonZeroSamples =
               clipLength > numPulledInputSamples ?
                  clipLength - numPulledInputSamples :
                  0u;
            for (auto i = 0u; i < numChannels; ++i)
            {
               std::fill(buffer[i], buffer[i] + numNonZeroSamples, 0.5f);
               std::fill(
                  buffer[i] + numNonZeroSamples, buffer[i] + samplesPerChannel,
                  0.f);
            }
            numPulledInputSamples += numNonZeroSamples;
         }
      };
      TimeAndPitchInterface::Parameters params;
      params.timeRatio = 0.5;
      StaffPadTimeAndPitch sut(numChannels, dcOffsetGetter, std::move(params));
      AudioContainer container(clipLength + 10, numChannels);
      sut.GetSamples(container.get(), clipLength + 10);
   }
}
