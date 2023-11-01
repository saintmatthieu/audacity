#include "GetOdf.h"

#include "CircularClip.h"
#include "ClipAnalysisUtils.h"
#include "ClipInterface.h"
#include "OnsetDetector.h"
#include "ResamplingClip.h"

#include <cmath>

namespace ClipAnalysis
{
ODF GetOdf(const ClipInterface& clip)
{
   const auto playDur = clip.GetPlayEndTime() - clip.GetPlayStartTime();
   // If `clip` is a loop, we can improve the analysis by treating it as such.
   // We also would like a power-of-two number of analyses, because we'll need
   // to take the auto-correlation of it, which can also be done efficiently
   // with an FFT.
   // 1. The number of analyses is a power of two such that our hop size
   // approximately 10ms.
   constexpr auto overlap = 2;
   constexpr auto targetFftDur = 0.1;
   const int k = 1 << static_cast<int>(
                    std::round(std::log2(playDur * overlap / targetFftDur)));
   const double fftDur = overlap * playDur / k;
   const auto fftRate = k / playDur;
   // To satisfy this `fftDur` as well as the power-of-two constraint, we will
   // need to resample. We don't need much more than 16kHz in the end.
   const int fftSize =
      1 << static_cast<int>(std::ceil(std::log2(fftDur * 16000)));
   const auto resampleRate = static_cast<double>(fftSize) / fftDur;
   const auto resampleFactor = resampleRate / clip.GetRate();
   CircularClip circularClip { clip,
                               static_cast<int>(fftSize / resampleFactor + .5),
                               overlap };
   ResamplingClip stftClip { circularClip, resampleRate };

   constexpr auto istftNeeded = false;
   OnsetDetector detector { fftSize, fftRate, istftNeeded };

   constexpr auto historyLength = 1u;
   detector.Start(historyLength);

   bool bLoopSuccess = true;
   sampleCount samplePos = 0;
   const auto numSamples = stftClip.GetVisibleSampleCount();
   std::optional<AudioSegmentSampleView> sampleCacheHolder;
   std::vector<float> buffer(fftSize / overlap);
   const auto processor = std::bind(
      &OnsetDetector::WindowProcessor, &detector, std::placeholders::_1);
   while (bLoopSuccess && samplePos < numSamples)
   {
      constexpr auto mayThrow = false;
      const auto numSamplesToGet =
         limitSampleBufferSize(fftSize / overlap, numSamples - samplePos);
      sampleCacheHolder.emplace(
         stftClip.GetSampleView(0u, samplePos, numSamplesToGet, mayThrow));
      const auto numNewSamples = sampleCacheHolder->GetSampleCount();
      assert(numNewSamples <= buffer.size());
      sampleCacheHolder->Copy(buffer.data(), numNewSamples);
      constexpr auto writeOutput = false;
      if (writeOutput)
      {
         static std::ofstream ofs {
            "C:/Users/saint/Downloads/resamplingOutput.txt"
         };
         for (auto i = 0u; i < numNewSamples; ++i)
            ofs << buffer[i] << std::endl;
      }
      bLoopSuccess =
         detector.ProcessSamples(processor, buffer.data(), numNewSamples);
      samplePos += numNewSamples;
   }

   detector.Finish(processor);

   const auto& odfs = detector.GetOnsetDetectionResults();
   assert(odfs.size() == k); // That's the whole point of the resampling, having
                             // control over the number of analyses.

   return { odfs, playDur };
}
} // namespace ClipAnalysis
