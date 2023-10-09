#include "ClipAnalysis.h"

#include "CircularSampleBuffer.h"
#include "ClipInterface.h"
#include "FFT.h"
#include "OnsetDetector.h"

#include <array>
#include <fstream>
#include <sstream>

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

inline double lagrange6(const double (&smp)[6], double t)
{
   auto* y = &smp[2];

   auto ym1_y1 = y[-1] + y[1];
   auto twentyfourth_ym2_y2 = 1.f / 24.f * (y[-2] + y[2]);
   auto c0 = y[0];
   auto c1 = 0.05f * y[-2] - 0.5f * y[-1] - 1.f / 3.f * y[0] + y[1] -
             0.25f * y[2] + 1.f / 30.f * y[3];
   auto c2 = 2.f / 3.f * ym1_y1 - 1.25f * y[0] - twentyfourth_ym2_y2;
   auto c3 = 5.f / 12.f * y[0] - 7.f / 12.f * y[1] + 7.f / 24.f * y[2] -
             1.f / 24.f * (y[-2] + y[-1] + y[3]);
   auto c4 = 0.25f * y[0] - 1.f / 6.f * ym1_y1 + twentyfourth_ym2_y2;
   auto c5 = 1.f / 120.f * (y[3] - y[-2]) + 1.f / 24.f * (y[-1] - y[2]) +
             1.f / 12.f * (y[1] - y[0]);

   // Estrin's scheme
   auto t2 = t * t;
   return (c0 + c1 * t) + (c2 + c3 * t) * t2 + (c4 + c5 * t) * t2 * t2;
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
   const auto N = odfVals.size();
   const auto M = 1 << static_cast<int>(std::ceil(std::log2(odfVals.size())));
   const double step = 1. * N / M;
   auto samp = 0.;
   std::vector<float> interpOdfVals(M);
   staffpad::audio::CircularSampleBuffer<double> ringBuff;
   ringBuff.setSize(N);
   ringBuff.writeBlock(0, N, odfVals.data());

   for (auto m = 0; m < M; ++m)
   {
      int n = samp;
      int start = n - 6;
      const auto frac = samp - n;
      samp += step;
      double smp[6];
      ringBuff.readBlock(n - 6, 6, smp);
      // interpOdfVals is floats because that's what RealFFT expects.
      interpOdfVals[m] = std::max<float>(0, lagrange6(smp, frac));
   }

   std::vector<float> powSpec(M);
   PowerSpectrum(M, interpOdfVals.data(), powSpec.data());
   // We need the entire power spectrum for the cross-correlation
   std::copy(
      powSpec.begin() + 1, powSpec.begin() + M / 2 - 1, powSpec.rbegin());
   std::vector<float> xcorr(M / 2 + 1);
   PowerSpectrum(M, powSpec.data(), xcorr.data());

   const auto lagPerSample = playDur / M;
   constexpr auto minBpm = 60;
   constexpr auto maxBpm = 180;
   const auto minSamps = 60. / maxBpm / lagPerSample;
   const auto maxSamps = 60. / minBpm / lagPerSample;

   // const auto winner = std::distance(
   //    scores.begin(),
   //    std::max_element(
   //       scores.begin(), scores.end(), [](const Score& a, const Score& b)
   //       {
   //          return a.xcorr + a.bpm < b.xcorr + b.bpm;
   //       }));

   std::ofstream ofs { "C:/Users/saint/Downloads/test.m" };
   ofs << "clear all, close all" << std::endl;
   ofs << PrintVector(odfVals, "odfVals");
   ofs << PrintVector(interpOdfVals, "interpOdfVals");
   ofs << PrintVector(powSpec, "powSpec");
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

   OnsetDetector detector { clip.GetRate() };

   constexpr auto historyLength = 1u;
   detector.Start(historyLength);

   bool bLoopSuccess = true;
   sampleCount samplePos = 0;
   const auto numSamples = clip.GetVisibleSampleCount();
   std::optional<AudioSegmentSampleView> sampleCacheHolder;
   constexpr auto blockSize = 4096u;
   std::array<float, blockSize> buffer;
   const auto processor = std::bind(
      &OnsetDetector::WindowProcessor, &detector, std::placeholders::_1);
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
