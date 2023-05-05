#include "WaveTrack.h"
#include "Project.h"
#include "SampleBlock.h"
#include "WaveClip.h"
#include "MockedPrefs.h"
#include "MockedAudio.h"

#include <catch2/catch.hpp>

#include <cmath>

namespace
{
std::vector<float> makeSquareWave()
{
   constexpr auto numSamples = 10u;
   std::vector<float> wave(numSamples);
   std::fill(wave.begin(), wave.begin() + numSamples / 2, 0.5f);
   std::fill(wave.begin() + numSamples / 2, wave.end(), -0.5f);
   return wave;
}

constexpr auto floatFormat = sampleFormat::floatSample;
} // namespace

TEST_CASE("WaveTrack")
{
   MockedAudio mockedAudio;
   MockedPrefs mockedPrefs;
   SECTION("GetStretched")
   {
      const auto project = AudacityProject::Create();
      const auto factory = SampleBlockFactory::New(*project);
      WaveTrack sut(factory, floatFormat, 1.0);
      const auto clip =
         std::make_shared<WaveClip>(factory, floatFormat, 1.0, 0);
      const auto squareWave = makeSquareWave();
      clip->SetSamples(
         reinterpret_cast<const char*>(squareWave.data()), floatFormat, 0,
         squareWave.size(), floatFormat);
      std::vector<float> output(squareWave.size() + 2);
      auto& processor = clip->GetProcessor();
      auto outputData = output.data();
      processor.Process(&outputData, 1u, output.size());
   }
}
