#include "StaffPadTimeAndPitch.h"
#include "AudioContainer.h"
#include "WavFileIO.h"

#include <catch2/catch.hpp>

namespace fs = std::filesystem;
using namespace std::literals::string_literals;

namespace
{
TimeAndPitchInterface::AudioSource
MakeAudioSource(const std::vector<std::vector<float>>& input)
{
   auto numPulledFrames = std::make_shared<unsigned long long>();
   *numPulledFrames = 0;
   return [&input,
           numPulledFrames](float* const* buffer, size_t samplesPerChannel) {
      const auto numFrames = input[0].size();
      const auto numChannels = input.size();
      const auto remainingSamples =
         numFrames > *numPulledFrames ? numFrames - *numPulledFrames : 0u;
      const size_t framesToRead = std::min(
         remainingSamples,
         static_cast<decltype(remainingSamples)>(samplesPerChannel));
      const auto numZerosToPad = samplesPerChannel - framesToRead;
      for (auto i = 0u; i < numChannels; ++i)
      {
         const auto in = input[i].data() + *numPulledFrames;
         std::copy(in, in + framesToRead, buffer[i]);
         std::fill(
            buffer[i] + framesToRead, buffer[i] + framesToRead + numZerosToPad,
            0.f);
      }
      *numPulledFrames += framesToRead;
   };
}
} // namespace

TEST_CASE("StaffPadTimeAndPitch")
{
   const auto inputPath =
      fs::path(CMAKE_SOURCE_DIR) / "tests/samples/AudacitySpectral.wav";
   std::optional<std::filesystem::path> outputDir;
   // Set this if you want to look at the output locally.
   // outputDir = "C:/Users/saint/Downloads/StaffPadTimeAndPitchTestOut";

   if (outputDir.has_value() && !fs::exists(*outputDir))
   {
      fs::create_directories(*outputDir);
   }

   SECTION("Smoke test")
   {
      std::vector<std::vector<float>> input;
      WavFileIO::Info info;
      REQUIRE(WavFileIO::Read(inputPath, input, info));
      for (const auto pitchRatio : std::vector<std::pair<int, int>> {
              { 4, 5 }, // major 3rd down
              { 1, 1 }, // no shift
              { 5, 4 }, // major 3rd up
           })
      {
         const auto pr =
            static_cast<double>(pitchRatio.first) / pitchRatio.second;
         for (const auto timeRatio : std::vector<std::pair<int, int>> {
                 { 1, 2 },
                 { 1, 1 },
                 { 2, 1 },
              })
         {
            const auto tr =
               static_cast<double>(timeRatio.first) / timeRatio.second;
            const auto numOutputFrames = info.numFrames * tr;
            AudioContainer container(numOutputFrames, info.numChannels);
            TimeAndPitchInterface::Parameters params;
            params.timeRatio = tr;
            params.pitchRatio = pr;
            StaffPadTimeAndPitch sut(
               info.numChannels, MakeAudioSource(input), std::move(params));
            sut.GetSamples(container.Get(), numOutputFrames);
            if (outputDir)
            {
               const auto fileName = inputPath.stem().concat(
                  "_t"s + std::to_string(timeRatio.first) + "-" +
                  std::to_string(timeRatio.second) + "_p" +
                  std::to_string(pitchRatio.first) + "-" +
                  std::to_string(pitchRatio.second) + ".wav");
               const auto outputPath = *outputDir / fileName;
               REQUIRE(WavFileIO::Write(
                  outputPath, container.vectorVector, info.sampleRate));
            }
         }
      }
   }

   SECTION("Not specifying time or pitch or ratio yields bit-exact results")
   {
      // Although this is no hard-proof, a bit-exact output is a strong evidence
      // that no processing happens if no time or pitch ratio is specified,
      // which is what we want.

      std::vector<std::vector<float>> input;
      WavFileIO::Info info;
      REQUIRE(WavFileIO::Read(inputPath, input, info));
      AudioContainer container(info.numFrames, info.numChannels);
      TimeAndPitchInterface::Parameters params;
      StaffPadTimeAndPitch sut(
         info.numChannels, MakeAudioSource(input), std::move(params));
      sut.GetSamples(container.Get(), info.numFrames);
      // Calling REQUIRE(container.vectorVector == input) directly for large
      // vectors might be rough on stdout in case of an error printout ...
      const auto outputEqualsInput = container.vectorVector == input;
      REQUIRE(outputEqualsInput);
   }
}
