#include "ResamplingClip.h"

#include "FloatVectorClip.h"
#include "MockedPrefs.h"
#include "WavFileIO.h"

#include <catch2/catch.hpp>

#include <numeric>

namespace ClipAnalysis
{
TEST_CASE("ResamplingClip")
{
   MockedPrefs prefs;

   SECTION("with tiny clip")
   {
      constexpr auto sampleRate = 160;
      std::vector<float> input(sampleRate);
      std::iota(input.begin(), input.end(), 0);
      FloatVectorClip clip { sampleRate, { input } };
      ResamplingClip resamplingClip { clip, 8000 };
      const auto numSamples = resamplingClip.GetVisibleSampleCount();
      auto numWrittenSamples = 0;
      std::vector<float> output(numSamples.as_size_t());
      while (numWrittenSamples < numSamples)
      {
         constexpr auto blockSize = 1024;
         const auto numToRead = std::min<int>(
            numSamples.as_size_t() - numWrittenSamples, blockSize);
         constexpr auto iChannel = 0;
         const auto view = resamplingClip.GetSampleView(
            iChannel, numWrittenSamples, numToRead);
         view.Copy(output.data() + numWrittenSamples, view.GetSampleCount());
         numWrittenSamples += view.GetSampleCount();
      }
   }

   SECTION("with real audio")
   {
      const auto inputPath =
         std::string(CMAKE_SOURCE_DIR) + "/tests/samples/AudacitySpectral.wav";
      WavFileIO::Info info;
      std::vector<std::vector<float>> input;
      REQUIRE(WavFileIO::Read(inputPath, input, info));
      FloatVectorClip clip { info.sampleRate, input };
      ResamplingClip resamplingClip { clip, 16000 };
      const auto numSamples = resamplingClip.GetVisibleSampleCount();
      auto numWrittenSamples = 0;
      std::vector<float> output(numSamples.as_size_t());
      while (numWrittenSamples < numSamples)
      {
         constexpr auto blockSize = 1024;
         const auto numToRead = std::min<int>(
            numSamples.as_size_t() - numWrittenSamples, blockSize);
         constexpr auto iChannel = 0;
         const auto view = resamplingClip.GetSampleView(
            iChannel, numWrittenSamples, numToRead);
         view.Copy(output.data() + numWrittenSamples, view.GetSampleCount());
         numWrittenSamples += view.GetSampleCount();
      }
      WavFileIO::Write(
         "C:/Users/saint/Downloads/ResamplingClipTestOut.wav", { output },
         16000);
   }
}
} // namespace ClipAnalysis
