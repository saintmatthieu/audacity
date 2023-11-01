#include "ClipAnalysisUtils.h"
#include "FloatVectorClip.h"
#include "GetBeatIndices.h"
#include "GetOdf.h"
#include "MockedPrefs.h"
#include "ODF.h"
#include "WavFileIO.h"

#include <catch2/catch.hpp>

#include <numeric>

namespace ClipAnalysis
{
namespace
{
ODF GetClipOdf(const char* filepath)
{
   std::vector<std::vector<float>> audio;
   WavFileIO::Info info;
   REQUIRE(WavFileIO::Read(filepath, audio, info));
   const FloatVectorClip clip { info.sampleRate, audio };
   return GetOdf(clip);
}

std::optional<std::vector<size_t>> GetClipBeatIndices(const char* filepath)
{
   const auto odf = GetClipOdf(filepath);
   const auto xcorr = GetNormalizedAutocorr(odf.values);
   return GetBeatIndices(odf, xcorr);
}
} // namespace

MockedPrefs prefs;

TEST_CASE("GetBeatIndices")
{
   SECTION("nothing else")
   {
      const auto indices = GetClipBeatIndices(
         "C:/Users/saint/Downloads/auto-tempo/nothing else.wav");
      REQUIRE(indices.has_value());
      REQUIRE(
         *indices == std::vector<size_t> { 0u, 11u, 21u, 32u, 43u, 53u, 64u,
                                           75u, 85u, 96u, 107u, 117u });
   }

   SECTION("drums")
   {
      const auto indices =
         GetClipBeatIndices("C:/Users/saint/Downloads/auto-tempo/drums.wav");
      REQUIRE(indices.has_value());
   }
}

TEST_CASE("IsLoop")
{
   SECTION("noise")
   {
      const auto odf =
         GetClipOdf("C:/Users/saint/Downloads/auto-tempo/noise.wav");
      const auto xcorr = GetNormalizedAutocorr(odf.values);
      const auto beatIndices = GetBeatIndices(odf, xcorr);
      // It'd be okay if `GetBeatIndices` failed for things that aren't loops,
      // but in that case either this test case should be modified or `IsLoop`
      // be removed.
      REQUIRE(beatIndices.has_value());
      const auto isLoop = IsLoop(odf, xcorr, *beatIndices);
      REQUIRE(!isLoop);
   }

   SECTION("nothing else")
   {
      const auto odf =
         GetClipOdf("C:/Users/saint/Downloads/auto-tempo/nothing else.wav");
      const auto xcorr = GetNormalizedAutocorr(odf.values);
      const auto beatIndices = GetBeatIndices(odf, xcorr);
      REQUIRE(beatIndices.has_value());
      const auto isLoop = IsLoop(odf, xcorr, *beatIndices);
      REQUIRE(isLoop);
   }

   SECTION("drums")
   {
      const auto odf =
         GetClipOdf("C:/Users/saint/Downloads/auto-tempo/drums.wav");
      const auto xcorr = GetNormalizedAutocorr(odf.values);
      const auto beatIndices = GetBeatIndices(odf, xcorr);
      REQUIRE(beatIndices.has_value());
      const auto isLoop = IsLoop(odf, xcorr, *beatIndices);
      REQUIRE(isLoop);
   }
}
} // namespace ClipAnalysis
