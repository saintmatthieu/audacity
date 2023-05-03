#include "ClientData.h"
#include "AudioSegment.h"
#include "SilenceSegment.h"
#include "WaveClip.h"
#include "WaveClipProcessor.h"

#include <catch2/catch.hpp>

class WaveClip;

class WaveTrack
{
public:
   void foo() const
   {
      const auto pSilenceProcessor = &mSilenceSegment->GetProcessor();
      const auto pFirstClipProcessor = &mFirstClipSegment->GetProcessor();
      const auto pLastCLipProcessor = &mLastClipSegment->GetProcessor();
      REQUIRE(pSilenceProcessor == &mSilenceSegment->GetProcessor());
      REQUIRE(pFirstClipProcessor == &mFirstClipSegment->GetProcessor());
      REQUIRE(pLastCLipProcessor == &mLastClipSegment->GetProcessor());
      REQUIRE(pFirstClipProcessor != pLastCLipProcessor);
   }

private:
   const std::shared_ptr<AudioSegment> mSilenceSegment =
      std::make_shared<SilenceSegment>();
   const std::shared_ptr<AudioSegment> mFirstClipSegment =
      std::make_shared<WaveClip>();
   const std::shared_ptr<AudioSegment> mLastClipSegment =
      std::make_shared<WaveClip>();
};

TEST_CASE("Stuff", "")
{
   WaveTrack track;
   SECTION("Does stuff")
   {
      track.foo();
   }
}
