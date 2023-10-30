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
std::optional<std::vector<size_t>> GetClipBeatIndices(const char* filepath)
{
   std::vector<std::vector<float>> audio;
   WavFileIO::Info info;
   REQUIRE(WavFileIO::Read(filepath, audio, info));
   const FloatVectorClip clip { info.sampleRate, audio };
   const auto odf = GetOdf(clip);
   return GetBeatIndices(odf);
}
} // namespace

TEST_CASE("GetBeatIndices")
{
   MockedPrefs prefs;

   SECTION("nothing else")
   {
      const auto indices =
         GetClipBeatIndices("C:/Users/saint/Downloads/auto-tempo/nothing else.wav");
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
} // namespace ClipAnalysis
