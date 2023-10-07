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

std::optional<double> GetBpm(const std::vector<double>& odfVals, double playDur)
{
   constexpr auto maxNumBars = 8u;
   struct Score
   {
      double xcorr;
      double bpm;
   };

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
      Score score { 10 * std::log(xcorr / numBeats), GetBpmLogLikelihood(bpm) };
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
