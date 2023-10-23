#include "CircularClip.h"

#include "FloatVectorClip.h"

#include <catch2/catch.hpp>

namespace ClipAnalysis
{
TEST_CASE("CircularClipTests")
{
   constexpr auto sampleRate = 10;
   constexpr auto fftSize = 4;
   constexpr auto overlap = 2;
   FloatVectorClip clip { sampleRate, { { 0, 1, 2, 3 } } };
   CircularClip circularClip { clip, fftSize, overlap };
   constexpr auto iChannel = 0;

   SECTION("at zero")
   {
      const auto view = circularClip.GetSampleView(iChannel, 0, 4);
      std::vector<float> output(view.GetSampleCount());
      view.Copy(output.data(), view.GetSampleCount());
      REQUIRE(output == std::vector<float> { 2, 3, 0, 1 });
   }

   SECTION("at end")
   {
      const auto view = circularClip.GetSampleView(iChannel, 4, 4);
      std::vector<float> output(view.GetSampleCount());
      view.Copy(output.data(), view.GetSampleCount());
      REQUIRE(output == std::vector<float> { 2, 3, 0, 1 });
   }

   SECTION("at middle")
   {
      const auto view = circularClip.GetSampleView(iChannel, 2, 4);
      std::vector<float> output(view.GetSampleCount());
      view.Copy(output.data(), view.GetSampleCount());
      REQUIRE(output == std::vector<float> { 0, 1, 2, 3 });
   }
}
} // namespace ClipAnalysis
