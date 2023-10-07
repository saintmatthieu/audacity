#include "ClipAnalysis.h"

#include "ClipInterface.h"
#include "ClipOnsetDetector.h"

#include <array>

namespace
{

double GetBpmLogLikelihood(double bpm)
{
   constexpr auto mu = 115.;
   constexpr auto alpha = 25.;
   const auto arg = (bpm - mu) / alpha;
   return -0.5 * arg * arg;
}

// Assuming that odfVals are onsets of a loop. Then there must be a round number
// of beats. Currently assuming a 4/4 time signature, it just tried a range of
// number of measures and picks the most likely one. Which is ?..
//
// Fact number 1: tempi abide by a distribution that might well be approximated
// with a gaussian PDF. Looking up some numbers on the net, this could have an
// average around 115 bpm and a variance of 25 bpm. This is given a weight in
// the comparison of hypotheses.
//
// Second, in most musical genres, and in any case those we expect to see the
// most in a DAW, a piece is a recursive rhythmic construction: the sections of
// a piece may have common time divisors ; the number of bars in a section is
// often a multiple of 2 ; and then a bar, as time signatures suggest, is
// divisible most often in 4 (4/4), sometimes in 3 (3/4, 6/8) and sometimes
// in 2.
//
// Here we begin simple and assume 4/4. Remains just finding the number of bars.
// The rest is currently an improvisation using the auto correlation of the
// onset detection values, which works halfway, but something more sensible and
// discriminant might be at hand ...

std::optional<double> GetBpm(const std::vector<double>& odfVals, double playDur)
{
   constexpr auto maxNumBars = 8u;
   struct Score
   {
      double xcorr;
      double bpm;
   };

   // The approach below may not be that discriminant, but first results looking
   // okay were obtained with it.
   std::vector<Score> scores;
   for (auto numBars = 1u; numBars <= maxNumBars; ++numBars)
   {
      const auto numBeats = numBars * 4;
      auto xcorr = 0.;
      for (auto beat = 1; beat <= numBeats; ++beat)
      {
         const auto offset = beat * odfVals.size() / numBeats;
         auto rotated = odfVals;
         std::rotate(rotated.begin(), rotated.begin() + offset, rotated.end());
         auto k = 0;
         for (auto v : rotated)
            xcorr += v * odfVals[k++];
      }
      const auto bpm = 4 * 60 * numBars / playDur;
      // This weight was chosen just by looking at xcorr and bpm scores while
      // debugging. We might get something yielding better results through some
      // regression technique.
      constexpr auto xcorrWeight = 10;
      Score score { xcorrWeight * std::log(xcorr / numBeats),
                    GetBpmLogLikelihood(bpm) };
      scores.push_back(score);
   }

   const auto winner = std::distance(
      scores.begin(),
      std::max_element(
         scores.begin(), scores.end(), [](const Score& a, const Score& b) {
            return a.xcorr + a.bpm < b.xcorr + b.bpm;
         }));

   const auto numBars = winner + 1;

   return 4 * numBars * 60 / playDur;
}
} // namespace

namespace ClipAnalysis
{
std::optional<double> GetBpm(const ClipInterface& clip)
{
   const auto playDur = clip.GetPlayEndTime() - clip.GetPlayStartTime();
   if (playDur <= 0)
      return {};

   ClipOnsetDetector detector { clip };

   constexpr auto historyLength = 1u;
   detector.Start(historyLength);

   bool bLoopSuccess = true;
   sampleCount samplePos = 0;
   const auto numSamples = clip.GetVisibleSampleCount();
   std::optional<AudioSegmentSampleView> sampleCacheHolder;
   constexpr auto blockSize = 4096u;
   std::array<float, blockSize> buffer;
   const auto processor = std::bind(
      &ClipOnsetDetector::ClipWindowProcessor, &detector,
      std::placeholders::_1);
   while (bLoopSuccess && samplePos < numSamples)
   {
      constexpr auto mayThrow = false;
      sampleCacheHolder.emplace(clip.GetSampleView(
         0u, samplePos,
         limitSampleBufferSize(blockSize, numSamples - samplePos), mayThrow));
      sampleCacheHolder->Copy(buffer.data(), buffer.size());
      const auto numNewSamples = sampleCacheHolder->GetSampleCount();
      bLoopSuccess =
         detector.ProcessSamples(processor, buffer.data(), numNewSamples);
      samplePos += numNewSamples;
   }

   detector.Finish(processor);

   return ::GetBpm(detector.GetOnsetDetectionResults(), playDur);
}
} // namespace ClipAnalysis
