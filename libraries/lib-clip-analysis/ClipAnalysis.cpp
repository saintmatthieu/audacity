#include "ClipAnalysis.h"

#include "CircularClip.h"
#include "ClipAnalysisUtils.h"
#include "GetBpmFromOdfXcorr.h"
#include "OnsetDetector.h"
#include "ResamplingClip.h"

#include <fstream>
#include <numeric>
#include <sstream>

namespace ClipAnalysis
{
namespace
{

double GetBpmLogLikelihood(double bpm)
{
   constexpr auto mu = 115.;
   constexpr auto alpha = 25.;
   const auto arg = (bpm - mu) / alpha;
   return -0.5 * arg * arg;
}

template <typename T>
std::string PrintVector(const T& vector, const std::string& name)
{
   std::stringstream ss;
   ss << name << " = [";
   auto separator = "";
   for (auto val : vector)
   {
      ss << separator << val;
      separator = ", ";
   }
   ss << "];" << std::endl;
   return ss.str();
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
   const auto xcorr = ClipAnalysis::GetNormalizedAutocorr(odfVals);
   const int M = 2 * (xcorr.size() - 1);
   const auto lagPerSample = playDur / M;
   constexpr auto minBpm = 60;
   constexpr auto maxBpm = 180;
   const auto minSamps = 60. / maxBpm / lagPerSample;
   const auto maxSamps = 60. / minBpm / lagPerSample;

   std::ofstream ofs { "C:/Users/saint/Downloads/test.m" };
   ofs << "clear all, close all" << std::endl;
   ofs << PrintVector(odfVals, "odfVals");
   ofs << PrintVector(xcorr, "xcorr");
   ofs << "plot(odfVals), grid" << std::endl;
   ofs
      << "hold on, plot((0:length(interpOdfVals)-1)*length(odfVals)/length(interpOdfVals), interpOdfVals, 'r--'), hold off, grid"
      << std::endl;
   ofs << "lagPerSample = " << lagPerSample << ";\n";
   ofs << "minLag = " << minSamps * lagPerSample << ";\n";
   ofs << "maxLag = " << maxSamps * lagPerSample << ";\n";
   ofs << "figure, plot((0:length(xcorr)-1)*lagPerSample, xcorr)\n";
   ofs << "hold on, plot([minLag, minLag], [0, max(xcorr)])\n";
   ofs << "plot([maxLag, maxLag], [0, max(xcorr)]), hold off\n";

   const auto groundTruth = 32;

   const auto winner = 0;
   const auto numBars = winner + 1;

   return ClipAnalysis::GetBpmFromOdfXcorr(xcorr, playDur);
}
} // namespace

std::optional<double> GetBpm(const ClipInterface& clip)
{
   const auto playDur = clip.GetPlayEndTime() - clip.GetPlayStartTime();
   if (playDur <= 0)
      return {};

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
   return GetBpm(detector.GetOnsetDetectionResults(), playDur);
}
} // namespace ClipAnalysis
