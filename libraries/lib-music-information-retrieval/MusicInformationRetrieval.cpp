/*  SPDX-License-Identifier: GPL-2.0-or-later */
/*!********************************************************************

  Audacity: A Digital Audio Editor

  MusicInformationRetrieval.cpp

  Matthieu Hodgkinson

**********************************************************************/
#include "MusicInformationRetrieval.h"
#include "BeatFinder.h"
#include "FFT.h"
#include "GetBeatFittingCoefficients.h"
#include "GetBeats.h"
#include "GetVampFeatures.h"
#include "MirAudioSource.h"

#include <dsp/mfcc/MFCC.h>

#include <array>
#include <cassert>
#include <cmath>
#include <fstream>
#include <numeric>
#include <regex>

namespace MIR
{
namespace
{
constexpr auto pi = 3.141592653589793;

auto RemovePathPrefix(const std::string& filename)
{
   return filename.substr(filename.find_last_of("/\\") + 1);
}

// About right for a 4/4, which is the most common time signature.
// When we get more clever, we may have different BPMs for different signatures,
// i.e., about 64 BPM for 6/8, and 140 for 3/4.
constexpr auto bpmExpectedValue = 120.;

// When we get time-signature estimate, we may need a map for that, since 6/8
// has 1.5 quarter notes per beat.
constexpr auto quarternotesPerBeat = 1.;

void FillMetadata(
   const std::string& filename, const MirAudioSource& source,
   std::optional<double>& bpm, std::optional<double>& offset)
{
   const auto bpmFromFilename = GetBpmFromFilename(filename);
   if (bpmFromFilename.has_value())
      bpm = bpmFromFilename;
   else
   {
      auto beatFinder = BeatFinder::CreateInstance(
         source, BeatTrackingAlgorithm::QueenMaryBarBeatTrack);
      const auto& beatInfo = beatFinder->GetBeats();
      if (beatInfo.has_value())
         GetBpmAndOffset(
            GetNormalizedAutocorrCurvatureRms(
               beatFinder->GetOnsetDetectionFunction()),
            *beatInfo, bpm, offset);
   }
}

template <typename T>
std::vector<int> GetOnsetIndices(const std::vector<T>& odf)
{
   // constexpr auto maxCount = 10;
   constexpr auto maxCount = std::numeric_limits<size_t>::max();
   std::vector<int> peakIndices;
   for (auto i = 1; i < odf.size() - 1; ++i)
      if (odf[i - 1] < odf[i] && odf[i] > odf[i + 1])
         peakIndices.push_back(i);
   if (peakIndices.size() > maxCount)
   {
      std::sort(peakIndices.begin(), peakIndices.end(), [&](int a, int b) {
         return odf[a] > odf[b];
      });
      peakIndices.erase(peakIndices.begin() + maxCount, peakIndices.end());
      std::sort(peakIndices.begin(), peakIndices.end());
   }

   return peakIndices;
}
} // namespace

MusicInformation::MusicInformation(
   const std::string& filename, double duration, const MirAudioSource& source)
    : filename { RemovePathPrefix(filename) }
    , duration { duration }
{
   FillMetadata(
      filename, source, const_cast<std::optional<double>&>(mBpm),
      const_cast<std::optional<double>&>(mOffset));
}

MusicInformation::operator bool() const
{
   // For now suffices to say that we have detected music content if there is
   // rhythm.
   return mBpm.has_value();
}

ProjectSyncInfo MusicInformation::GetProjectSyncInfo(
   const std::optional<double>& projectTempo) const
{
   assert(*this);
   if (!*this)
      return { 0., 0. };

   const auto error = *mBpm - bpmExpectedValue;

   const auto qpm = *mBpm * quarternotesPerBeat;

   auto recommendedStretch = 1.0;
   if (projectTempo.has_value())
   {
      const auto diff = *projectTempo - qpm;
      const auto diffHalf = *projectTempo - qpm / 2;
      const auto diffDouble = *projectTempo - qpm * 2;
      if (diffDouble * diffDouble < diff * diff)
         recommendedStretch = 2;
      else if (diffHalf * diffHalf < qpm)
         recommendedStretch = 0.5;
   }

   auto excessDurationInQuarternotes = 0.;
   auto numQuarters = duration * qpm / 60.;
   const auto roundedNumQuarters = std::round(numQuarters);
   const auto delta = numQuarters - roundedNumQuarters;
   // If there is an excess less than a 32nd, we treat it as an edit error.
   if (0 < delta && delta / 8)
      excessDurationInQuarternotes = delta;

   auto offsetInQuarternotes = 0.;
   if (mOffset.has_value())
      offsetInQuarternotes = *mOffset * qpm / 60.;

   return { qpm, recommendedStretch, excessDurationInQuarternotes,
            offsetInQuarternotes };
}

std::optional<double> GetBpmFromFilename(const std::string& filename)
{
   // regex matching a forward or backward slash:

   // Regex: <(anything + (directory) separator) or nothing> <2 or 3 digits>
   // <optional separator> <bpm (case-insensitive)> <separator or nothing>
   const std::regex bpmRegex {
      R"((?:.*(?:_|-|\s|\.|/|\\))?(\d+)(?:_|-|\s|\.)?bpm(?:(?:_|-|\s|\.).*)?)",
      std::regex::icase
   };
   std::smatch matches;
   if (std::regex_match(filename, matches, bpmRegex))
      try
      {
         const auto value = std::stoi(matches[1]);
         return 30 <= value && value <= 300 ? std::optional<double> { value } :
                                              std::nullopt;
      }
      catch (const std::invalid_argument& e)
      {
         assert(false);
      }
   return {};
}

void GetBpmAndOffset(
   double odfXcorrCurvRms, const BeatInfo& beatInfo, std::optional<double>& bpm,
   std::optional<double>& offset,
   std::optional<ClassifierTuningThresholds> tuningThresholds)
{
   if (!IsRhythmic(
          odfXcorrCurvRms,
          tuningThresholds.has_value() ?
             std::optional<double> { tuningThresholds->isRhythmic } :
             std::nullopt))
      return;
   const auto coefs =
      GetBeatFittingCoefficients(beatInfo.beatTimes, beatInfo.indexOfFirstBeat);
   const auto rms = GetBeatFittingErrorRms(coefs, beatInfo);
   if (!HasConstantTempo(
          rms, tuningThresholds.has_value() ?
                  std::optional<double> { tuningThresholds->hasConstantTempo } :
                  std::nullopt))
      return;
   const auto& [alpha, beta] = coefs;
   bpm = 60. / alpha;
   offset = -beta;
}

double GetBeatFittingErrorRms(
   const std::pair<double, double>& coefs, const BeatInfo& info)
{
   const auto& [alpha, beta] = coefs;
   // Estimate variance of fit error:
   const auto k0 = info.indexOfFirstBeat.value_or(0);
   std::vector<int> k(info.beatTimes.size());
   std::iota(k.begin(), k.end(), k0);
   std::vector<double> squaredErrors(info.beatTimes.size());
   std::transform(
      k.begin(), k.end(), info.beatTimes.begin(), squaredErrors.begin(),
      [alpha, beta](int k, double t) {
         const auto err = t - alpha * k - beta;
         return err * err;
      });
   return std::sqrt(
      std::accumulate(squaredErrors.begin(), squaredErrors.end(), 0.) /
      static_cast<double>(squaredErrors.size()));
}

bool IsRhythmic(double beatSnr, std::optional<double> tuningThreshold)
{
   constexpr auto defaultThreshold = 6e+6;
   const auto threshold = tuningThreshold.value_or(defaultThreshold);
   return beatSnr > threshold;
}

bool HasConstantTempo(
   double beatFittingErrorRms, std::optional<double> tuningThreshold)
{
   // If the average error is more than something noticeable, i.e., a 16th of a
   // beat (at most a 32nd, more likely a 64th), then don't trust the fit.
   constexpr auto meanBeatErrorThreshold = 1. / 16;
   const auto meanThreshold =
      tuningThreshold.value_or(meanBeatErrorThreshold * meanBeatErrorThreshold);
   return beatFittingErrorRms < meanThreshold;
}

double GetNormalizedAutocorrCurvatureRms(const std::vector<double>& x)
{
   assert(x.size() > 2);
   if (x.size() < 3)
      return 0.;
   auto curvature = GetNormalizedAutocorrelation(x);
   std::adjacent_difference(
      curvature.begin(), curvature.end(), curvature.begin());
   std::adjacent_difference(
      curvature.begin(), curvature.end(), curvature.begin());
   return std::sqrt(
      std::accumulate(
         curvature.begin() + 2, curvature.end(), 0.,
         [](double a, double b) { return a + b * b; }) /
      static_cast<double>(curvature.size()));
}

double GetBeatSnr(
   const std::vector<double>& odf, double odfSampleRate,
   const std::vector<double>& beatTimes)
{
   // We have an onset-detection function `odf` sampled at `odfSampleRate`.
   // For each entry in `beatTimes`, we want to find the closest peak in `odf`.
   // Then we can take the SNR between the beat peak power and the power of the
   // odf at non-peak indices.

   std::vector<size_t> peakIndices(beatTimes.size());
   std::transform(
      beatTimes.begin(), beatTimes.end(), peakIndices.begin(),
      [odf, odfSampleRate](double t) {
         auto j = static_cast<int>(t * odfSampleRate + 0.5);
         // Find closest peak, circling around if necessary.
         while (true)
         {
            const auto i = (j - 1) % odf.size();
            const auto k = (j + 1) % odf.size();
            if (odf[i] <= odf[j] && odf[j] >= odf[k])
               break;
            j = odf[i] > odf[k] ? i : k;
         }
         return j;
      });

   // Take the set difference between all odf indices and the peak indices.
   std::vector<size_t> allIndices(odf.size());
   std::iota(allIndices.begin(), allIndices.end(), 0);
   std::vector<size_t> nonPeakIndices;
   std::set_difference(
      allIndices.begin(), allIndices.end(), peakIndices.begin(),
      peakIndices.end(), std::back_inserter(nonPeakIndices));

   const auto odfPower =
      std::accumulate(
         nonPeakIndices.begin(), nonPeakIndices.end(), 0.,
         [&](double a, size_t i) { return a + odf[i] * odf[i]; }) /
      nonPeakIndices.size();

   const auto odfPeaksPower =
      std::accumulate(
         peakIndices.begin(), peakIndices.end(), 0.,
         [&](double a, size_t i) { return a + odf[i] * odf[i]; }) /
      peakIndices.size();

   return std::log(odfPeaksPower / odfPower);
}

std::optional<std::vector<size_t>>
GetBeatIndices(const ODF& odf, const std::vector<float>& xcorr)
{
   const auto lagPerSample = odf.duration / xcorr.size();
   constexpr auto minBpm = 60;
   constexpr auto maxBpm = 600; // Actually we're not looking for beats, but we
                                // are considering tatums, too.
   const auto minSamps = static_cast<int>(60. / maxBpm / lagPerSample + .5);
   const auto maxSamps = static_cast<int>(60. / minBpm / lagPerSample + .5);

   // Get first peak after minLag whose value is larger than the xcorr average:
   const auto avg = std::accumulate(xcorr.begin(), xcorr.end(), 0.) /
                    static_cast<double>(xcorr.size());
   const auto end = xcorr.begin() + maxSamps;

   // Find index of largest value in `xcorr` in the range [minSamps, maxSamps].
   const auto maxIt = std::max_element(
      xcorr.begin() + minSamps, end, [](auto a, auto b) { return a < b; });

   if (maxIt == end || *maxIt < avg)
      return {};

   auto P = std::distance(xcorr.begin(), maxIt);
   std::vector<size_t> beatIndices { 0u };
   auto prevP = 0;
   auto i = 0;
   while (true)
   {
      // peak index
      auto j =
         i == 0 ?
            P :
            static_cast<size_t>(static_cast<double>(prevP) * (i + 1) / i + .5);
      if (j >= xcorr.size())
         break;
      while (true)
      {
         const auto a = (j - 1) % xcorr.size();
         const auto b = (j + 1) % xcorr.size();
         if (xcorr[a] <= xcorr[j] && xcorr[j] >= xcorr[b])
            break;
         j = xcorr[a] > xcorr[j] ? a : b;
      }
      if (j < prevP)
         break;
      else if (j == prevP)
         // Doesn't look good ; just give up.
         return {};
      beatIndices.push_back(j);
      prevP = j;
      ++i;
   }
   return beatIndices;
}

bool IsLoop(
   const ODF& odf, const std::vector<float>& xcorr,
   const std::vector<size_t>& beatIndices)
{
   // Should be normalized.
   std::vector<int> divisors { 2, 3, 4, 6, 8, 9, 12 };
   const auto numBeats = beatIndices.size();
   // Filter out divisors that aren't divisors of the number of beats
   divisors.erase(
      std::remove_if(
         divisors.begin(), divisors.end(),
         [&](auto divisor) { return numBeats % divisor != 0; }),
      divisors.end());
   // If there isn't a divisor that satisfies this, we're done.
   if (divisors.empty())
      return false;

   std::vector<double> divisorPeriods(divisors.size());
   std::transform(
      divisors.begin(), divisors.end(), divisorPeriods.begin(),
      [&](auto divisor) {
         return static_cast<double>(xcorr.size()) / divisor;
      });
   const auto xcorrLagStep = odf.duration / xcorr.size();
   // In seconds
   std::vector<double> averageDivisorBeatDistances(divisors.size());
   std::transform(
      divisorPeriods.begin(), divisorPeriods.end(),
      averageDivisorBeatDistances.begin(), [&](double divisorPeriod) {
         std::vector<double> beatIndexDistances(numBeats);
         std::transform(
            beatIndices.begin(), beatIndices.end(), beatIndexDistances.begin(),
            [&](auto beatIndex) {
               // Find multiple of divisor period closest to i
               const auto closestDivisorPeriodMultiple =
                  std::round(beatIndex / divisorPeriod) * divisorPeriod;
               const auto distance =
                  (beatIndex - closestDivisorPeriodMultiple) * xcorrLagStep;
               return distance < 0 ? -distance : distance;
            });
         return std::accumulate(
                   beatIndexDistances.begin(), beatIndexDistances.end(), 0.) /
                numBeats;
      });

   std::vector<float> averageDivisorValues(divisors.size());
   std::transform(
      divisors.begin(), divisors.end(), averageDivisorValues.begin(),
      [&](auto divisor) {
         std::vector<float> divisorValues(divisor);
         const auto step = numBeats / divisor;
         for (auto i = 0; i < divisor; ++i)
            divisorValues[i] = xcorr[beatIndices[i * step]];
         return std::accumulate(
                   divisorValues.begin(), divisorValues.end(), 0.f) /
                divisor;
      });

   const auto xcorrMean = std::accumulate(xcorr.begin(), xcorr.end(), 0.) /
                          static_cast<double>(xcorr.size());
   std::vector<double> divisorScores(divisors.size());
   auto i = 0;
   std::for_each(divisorScores.begin(), divisorScores.end(), [&](auto& score) {
      // Scale the distance by -10 to get the log10 of the probability.
      // This means that a distance of 0.1 second yields a probabilty of 0.1.
      const auto distanceLog10Prob = -10 * averageDivisorBeatDistances[i];

      // y(x) = a*log(b*x), with y(1) = 1 and y(0.1) = 0.5. This reduces to
      // y(x) = aÜ(log(x) + 1/a), with a = -1/(2*log(1/10));
      constexpr auto a = 0.217147240951626;
      const auto divisorValue = averageDivisorValues[i];
      const auto x = std::clamp(divisorValue - xcorrMean, 0., 1.);
      const auto peakinessProb = a * (std::log(x) + 1 / a);
      const auto peakinessLog10Prob = std::log10(peakinessProb);
      score = distanceLog10Prob + peakinessLog10Prob;
      ++i;
   });

   const auto bestScore = *std::max_element(
      divisorScores.begin(), divisorScores.end(),
      [](auto a, auto b) { return a < b; });

   return bestScore > -1;
}

std::vector<float>
GetNormalizedAutocorrelation(const std::vector<double>& x, bool full)
{
   const auto N = x.size();
   const auto M = 1 << static_cast<int>(std::ceil(std::log2(x.size())));
   // Interpolate linearly and circularly x to get xi. It'll have sufficient
   // precision for this purpose.
   auto ni = 0.;
   std::vector<float> xi(M);
   for (auto m = 0; m < M; ++m)
   {
      const auto n = static_cast<float>(m) * N / M;
      const auto n0 = static_cast<int>(n);
      assert(n0 < N);            // This should be true ...
      const auto x0 = x[n0 % N]; // ... but still.
      const auto x1 = x[(n0 + 1) % N];
      const auto frac = n - n0;
      xi[m] = x0 * (1 - frac) + x1 * frac;
   }

   std::vector<float> powSpec(M);
   PowerSpectrum(M, xi.data(), powSpec.data());
   // We need the entire power spectrum for the cross-correlation, not only the
   // left-hand side.
   std::copy(
      powSpec.begin() + 1, powSpec.begin() + M / 2 - 1, powSpec.rbegin());
   std::vector<float> xcorr(M);
   InverseRealFFT(M, powSpec.data(), nullptr, xcorr.data());
   if (!full)
      // Discard symmetric right-hand side of xcorr:
      xcorr.erase(xcorr.begin() + M / 2 + 1, xcorr.end());
   const auto normalizer = 1 / xcorr[0];
   std::transform(
      xcorr.begin(), xcorr.end(), xcorr.begin(),
      [normalizer](float x) { return x * normalizer; });
   return xcorr;
}

std::vector<float>
GetNormalizedAutocorrelation(const std::vector<float>& x, bool full)
{
   std::vector<double> xDouble(x.begin(), x.end());
   return GetNormalizedAutocorrelation(xDouble, full);
}

namespace
{
template <typename T, typename U = T>
std::vector<U> UpsampleByLinearInterpolation(const std::vector<T>& x, size_t M)
{
   const auto N = x.size();
   std::vector<U> upsampled(M);
   const auto step = static_cast<double>(N) / M;
   auto n = 0.;
   std::for_each(upsampled.begin(), upsampled.end(), [&](U& u) {
      const auto n0 = static_cast<int>(n);
      const auto u0 = x[n0 % N];
      const auto u1 = x[(n0 + 1) % N];
      const auto frac = n - n0;
      u = u0 * (1 - frac) + u1 * frac;
      n += step;
   });
   return upsampled;
}

template <typename T>
std::vector<std::vector<T>>
GetSelfSimilarityMatrix(const std::vector<std::vector<T>>& xx, int sampleRate)
{
   // Get the MFCC of each inner vector.
   std::vector<std::vector<double>> mfccs(xx.size());
   std::transform(
      xx.begin(), xx.end(), mfccs.begin(),
      [sampleRate](const std::vector<T>& x) {
         const auto M = 1 << static_cast<int>(std::ceil(std::log2(x.size())));
         const auto xi = UpsampleByLinearInterpolation<T, double>(x, M);
         const auto iSampleRate =
            static_cast<int>(1. * sampleRate * M / x.size() + .5);
         MFCCConfig config { iSampleRate };
         config.fftsize = M;
         MFCC mfcc { config };
         std::vector<double> y(config.nceps + 1);
         mfcc.process(xi.data(), y.data());
         return y;
      });
   // Now compute the self-similarity matrix of the MFCCs based on Euclidean
   // distance:
   std::vector<std::vector<T>> ssm(xx.size());
   std::transform(
      mfccs.begin(), mfccs.end(), ssm.begin(),
      [&](const std::vector<double>& x) {
         std::vector<T> row(xx.size());
         std::transform(
            mfccs.begin(), mfccs.end(), row.begin(),
            [&](const std::vector<double>& y) {
               return std::inner_product(
                  x.begin(), x.end(), y.begin(), 0.,
                  [](double a, double b) { return a + b; },
                  [](double a, double b) { return (a + -b) * (a - b); });
            });
         return row;
      });
   return ssm;
}

template <typename T>
void DoSSMStuff(const std::vector<T>& input, int sampleRate)
{
   // We will try all hypotheses of 1, 2, 3, 4, 6 or 8 bars of 4/4, 3/4 or 6/8.
   // Assuming the ODF is that of a loop, for each combination we divide the
   // ODF accordingly. E.g., for the combination of 2 bars of 6/8, we divide the
   // input in 2*8 = 16 vectors.
   constexpr std::array<int, 6> numBars { 1, 2, 3, 4, 6, 8 };
   constexpr std::array<int, 3> beatsPerBar = { 4, 3, 2 };
   std::vector<double> scores;

   constexpr auto log = false;
   std::ofstream ofs;
   if (log)
      ofs.open("C:/Users/saint/Downloads/mfcc_ssm.m");
   auto f = 1;
   std::for_each(numBars.begin(), numBars.end(), [&](int numBars) {
      std::transform(
         beatsPerBar.begin(), beatsPerBar.end(), std::back_inserter(scores),
         [&](int beatsPerBar) {
            const auto numBeats = numBars * beatsPerBar;
            std::vector<std::vector<float>> partitionedInput(numBeats);
            const int partitionSize = std::ceil(input.size() / numBeats);
            auto m = 0;
            std::for_each(
               partitionedInput.begin(), partitionedInput.end(),
               [&](std::vector<float>& partition) {
                  partition.resize(partitionSize);
                  const auto numSamplesToCopy = std::min<int>(
                     partitionSize, input.size() - m * partitionSize);
                  std::copy(
                     input.begin() + m * partitionSize,
                     input.begin() + m * partitionSize + numSamplesToCopy,
                     partition.begin());
                  std::fill(
                     partition.begin() + numSamplesToCopy, partition.end(), 0.);
                  ++m;
               });
            // Now we have `numBeats` vectors of size `partitionSize`. We can
            // now compute the self-similarity matrix of these vectors.
            const auto ssm =
               GetSelfSimilarityMatrix(partitionedInput, sampleRate);

            if (!log)
               return 0.;

            // Now log this result in a file, indicating the number of bars and
            // beats per bar:
            const std::string varName = std::string { "ssm" } +
                                        std::to_string(numBars) + "By" +
                                        std::to_string(beatsPerBar);
            ofs << varName + " = [";
            std::for_each(
               ssm.begin(), ssm.end(), [&](const std::vector<float>& row) {
                  std::for_each(
                     row.begin(), row.end(), [&](float x) { ofs << x << ","; });
                  ofs << std::endl;
               });
            ofs << "];" << std::endl;
            const std::string xticks = "(0:" + std::to_string(numBeats) + "-1)";
            ofs << "figure(" << std::to_string(f++) << "), imagesc(" << xticks
                << "+.5," << xticks << "+.5," << varName
                << "), axis equal, colorbar, grid" << std::endl;
            ofs << "set(gca, 'xtick', " << xticks << ",'ytick', " << xticks
                << ")" << std::endl;
            ofs << "title('" << varName << "')" << std::endl << std::endl;

            // For now, until we implement it.
            return 0.;
         });
   });
}
} // namespace

std::pair<double, double>
GetApproximateGcd(const std::vector<float>& odf, double odfSampleRate)
{
   const auto peakIndices = GetOnsetIndices(odf);
   std::ofstream ofsPeaks("C:/Users/saint/Downloads/log_peaks.txt");
   std::for_each(peakIndices.begin(), peakIndices.end(), [&](int i) {
      ofsPeaks << i << ",";
   });
   ofsPeaks << std::endl;

   struct Onset
   {
      int index;
      float weight;
   };

   std::vector<Onset> onsets;
   std::transform(
      peakIndices.begin(), peakIndices.end(), std::back_inserter(onsets),
      [&](int i) {
         return Onset { i, odf[i] };
      });
   // Normalize `onsets` so that they sum up to 1:
   const auto normalizer =
      1. / std::accumulate(
              onsets.begin(), onsets.end(), 0.,
              [](double a, const Onset& onset) { return a + onset.weight; });
   std::for_each(onsets.begin(), onsets.end(), [&](Onset& onset) {
      onset.weight *= normalizer;
   });

   // `onsets` was derived from audio which may be a musical loop or
   // not. We will therefore return our best guess together with a confidence
   // value.
   // IF the audio is a loop, we expect the first beat to be at time 0, hence we
   // don't expect an offset in the onset times.
   // We still have to try and find the approximate GCD. We may use
   // `onsets` as relative weights.

   // Trying with top-voted answer to
   // https://math.stackexchange.com/questions/1834472/approximate-greatest-common-divisor
   constexpr auto minBpm = 60;
   constexpr auto maxBpm = 600;
   const auto lagMin = static_cast<int>(odfSampleRate * 60 / maxBpm);
   const auto lagMax =
      std::min<int>(odf.size(), static_cast<int>(odfSampleRate * 60 / minBpm));
   std::vector<int> candidates(lagMax - lagMin + 1);
   std::iota(candidates.begin(), candidates.end(), lagMin);
   std::vector<double> gcdAppeal(candidates.size());
   std::transform(
      candidates.begin(), candidates.end(), gcdAppeal.begin(), [&](int period) {
         const auto meanSin = std::accumulate(
                                 peakIndices.begin(), peakIndices.end(), 0.,
                                 [period](double sum, int i) {
                                    return sum + std::sin(2 * pi * i / period);
                                 }) /
                              peakIndices.size();
         const auto meanCos = std::accumulate(
                                 peakIndices.begin(), peakIndices.end(), 0.,
                                 [period](double sum, int i) {
                                    return sum + std::cos(2 * pi * i / period);
                                 }) /
                              peakIndices.size();
         const auto tmp = meanCos - 1;
         return 1 - .5 * std::sqrt(meanSin * meanSin + tmp * tmp);
      });
   const auto winner = std::distance(
      gcdAppeal.begin(), std::max_element(gcdAppeal.begin(), gcdAppeal.end()));
   const auto gcd = candidates[winner];
   const auto errorRms = std::sqrt(
      std::accumulate(
         onsets.begin(), onsets.end(), 0.,
         [&](double sum, const Onset& onset) {
            const auto expected =
               std::round(static_cast<double>(onset.index) / gcd) * gcd;
            const auto error = (onset.index - expected) / odfSampleRate;
            return sum + error * error;
         }) /
      onsets.size());

   // Don't preserve the symmetric right-hand side of the autocorrelation as
   // this may intefere with the HPS.
   constexpr auto full = false;
   const auto xcorr = GetNormalizedAutocorrelation(odf, full);

   // Derive harmonic product spectrum (HPS) of xcorr
   constexpr auto maxNumHarmonics = 4;
   std::vector<float> ixcorr(xcorr.size() * maxNumHarmonics);
   // Interpolate linearly `xcorr` to get `ixcorr`.
   auto i = 0;
   std::for_each(xcorr.begin(), xcorr.end(), [&](float x) {
      const auto x0 = xcorr[i];
      const auto x1 = xcorr[(i + 1) % xcorr.size()];
      for (auto j = 0; j < maxNumHarmonics; ++j)
      {
         const auto frac = static_cast<float>(j) / maxNumHarmonics;
         const auto x = x0 * (1 - frac) + x1 * frac;
         ixcorr[i * maxNumHarmonics + j] = x;
      }
      ++i;
   });
   std::vector<float> hps(xcorr.size());
   std::copy(xcorr.begin(), xcorr.end(), hps.begin());
   for (auto i = 2; i <= maxNumHarmonics; ++i)
   {
      auto j = 0;
      std::for_each(
         hps.begin(), hps.end(), [&](float& x) { x *= ixcorr[i * j++]; });
   }
   // Find index of first trough in HPS:
   auto prev = std::numeric_limits<float>::max();
   const auto it = std::find_if(hps.begin(), hps.end(), [&](float x) {
      const auto found = x > prev;
      prev = x;
      return found;
   });
   const auto maxIt = std::max_element(it, hps.end());
   const auto maxIndex = std::distance(hps.begin(), maxIt);
   const auto xcorrSampleRate = 1. * odfSampleRate * xcorr.size() / odf.size();
   const auto beatPeriod = maxIndex / xcorrSampleRate / maxNumHarmonics;
   const auto bpm = 60. / beatPeriod;
   const auto confidence = std::pow(*maxIt, 1. / maxNumHarmonics);

   std::ofstream xcorrOfs("C:/Users/saint/Downloads/log_xcorr.txt");
   std::for_each(
      xcorr.begin(), xcorr.end(), [&](float x) { xcorrOfs << x << ","; });
   xcorrOfs << std::endl;

   std::ofstream hpsOfs("C:/Users/saint/Downloads/log_hps.txt");
   std::for_each(hps.begin(), hps.end(), [&](float x) { hpsOfs << x << ","; });
   hpsOfs << std::endl;

   return { bpm, confidence };
}

namespace
{
std::pair<int, int> GetHopAndFrameSizes(int sampleRate)
{
   // 1024 for sample rate 44.1kHz
   const auto hopSize =
      1 << (10 + (int)std::round(std::log2(sampleRate / 44100.)));
   return { hopSize, 2 * hopSize };
}

std::vector<double> MakeHann(size_t N)
{
   std::vector<double> w(N);
   for (auto n = 0; n < N; ++n)
      w[n] = .5 * (1 - std::cosf(2 * pi * n / N));
   const auto sum = std::accumulate(w.begin(), w.end(), 0.);
   std::transform(
      w.begin(), w.end(), w.begin(), [sum](float x) { return x / sum; });
   return w;
}
} // namespace

std::vector<float> GetOnsetDetectionFunction(
   const MirAudioSource& source, double& odfSampleRate,
   double smoothingThreshold)
{
   const auto sampleRate = source.GetSampleRate();
   const auto [hopSize, frameSize] = GetHopAndFrameSizes(sampleRate);
   std::vector<float> buffer(frameSize);
   std::vector<float> odf;
   const auto powSpecSize = frameSize / 2 + 1;
   std::vector<float> prevPowSpec(powSpecSize);
   std::fill(prevPowSpec.begin(), prevPowSpec.end(), 0.f);

   const auto hann = MakeHann(frameSize);

   std::ofstream ofsStft("C:/Users/saint/Downloads/log_stft.txt");

   long long start = -frameSize;
   auto isFirst = true;
   while (source.ReadFloats(buffer.data(), start, frameSize, start < 0) ==
          frameSize)
   {
      std::transform(
         buffer.begin(), buffer.end(), hann.begin(), buffer.begin(),
         [](float a, float b) { return a * b; });
      PowerSpectrum(frameSize, buffer.data(), buffer.data());

      // Normalize the spectrum
      std::transform(
         buffer.begin(), buffer.end(), buffer.begin(),
         [frameSize](float x) { return x / frameSize; });

      // Compress the frame as per Fundamentals of Music Processing, (6.5)
      constexpr auto gamma = 100.;
      std::transform(
         buffer.begin(), buffer.begin() + powSpecSize, buffer.begin(),
         [](float x) { return std::log(1 + gamma * std::sqrt(x)); });

      constexpr auto verticalSmoothing = false;
      if (verticalSmoothing)
      {
         assert(frameSize == 2048); // We'll need to generalize
         constexpr auto hannSize = 100;
         assert(hannSize % 2 == 0);
         const auto vHann = MakeHann(hannSize);
         for (auto n = 0; n < powSpecSize; ++n)
         {
            auto sum = 0.f;
            for (auto m = 0; m < hannSize; ++m)
            {
               const auto k = n + m - hannSize / 2;
               if (k >= 0 && k < powSpecSize)
                  sum += vHann[m] * buffer[k];
            }
            buffer[n] = sum;
         }
      }

      if (!isFirst)
      {
         auto k = 0;
         const auto sum = std::accumulate(
            buffer.begin(), buffer.begin() + powSpecSize, 0.,
            [&](float a, float mag) {
               // Half-wave-rectified stuff
               return a + std::max(0.f, mag - prevPowSpec[k++]);
            });
         std::for_each(
            buffer.begin(), buffer.begin() + powSpecSize,
            [&](float x) { ofsStft << x << ","; });
         ofsStft << std::endl;
         odf.push_back(sum);
      }
      std::copy(
         buffer.begin(), buffer.begin() + powSpecSize, prevPowSpec.begin());
      start += hopSize;
      isFirst = false;
   }

   std::ofstream ofsRawOdf("C:/Users/saint/Downloads/log_raw.txt");
   std::for_each(
      odf.begin(), odf.end(), [&](float x) { ofsRawOdf << x << ","; });
   ofsRawOdf << std::endl;

   constexpr auto M = 5;
   std::vector<float> window(2 * M + 1);
   for (auto n = 0; n < 2 * M + 1; ++n)
      window[n] = .5 * (1 - std::cosf(2 * pi * n / (2 * M + 1)));
   const auto windowSum = std::accumulate(window.begin(), window.end(), 0.);
   std::transform(
      window.begin(), window.end(), window.begin(),
      [windowSum](double w) { return w / windowSum; });
   auto n = 0;
   std::vector<double> movingAverage(odf.size());
   std::transform(odf.begin(), odf.end(), movingAverage.begin(), [&](float) {
      auto y = 0.;
      for (auto i = -M; i <= M; ++i)
         y += odf[(n + i) % odf.size()] * window[i + M];
      ++n;
      return y;
   });

   std::ofstream ofsAvg("C:/Users/saint/Downloads/log_avg.txt");
   std::for_each(movingAverage.begin(), movingAverage.end(), [&](double x) {
      ofsAvg << x << ",";
   });
   ofsAvg << std::endl;

   if (smoothingThreshold > 0.)
      // subtract moving average from odf:
      std::transform(
         odf.begin(), odf.end(), movingAverage.begin(), odf.begin(),
         [smoothingThreshold](float a, float b) {
            return std::max<float>(a - b * smoothingThreshold, 0.f);
         });

   odfSampleRate = static_cast<double>(sampleRate) / hopSize;
   return odf;
}

void NewStuff(const MirAudioSource& source)
{
   // Load all samples of `source` in a vector:
   std::vector<float> samples(source.GetNumSamples());
   source.ReadFloats(samples.data(), 0, samples.size(), false);
   DoSSMStuff(samples, source.GetSampleRate());
}

namespace
{
template <typename T>
std::vector<T>
GetCrossCorrelation(const std::vector<T>& x, const std::vector<T>& y)
{
   // We don't expect `x` and `y` to be large, doing it in the time domain
   // should be fine - for now.
   std::vector<T> xcorr(x.size());
   std::transform(x.begin(), x.end(), xcorr.begin(), [](T) { return 0.; });
   for (auto i = 0; i < x.size(); ++i)
      for (auto j = 0; j < y.size(); ++j)
      {
         const auto k = (i + j) % x.size();
         xcorr[i] += x[k] * y[j];
      }
   return xcorr;
}
} // namespace

double Experiment1(const std::vector<float>& odf, double odfSampleRate)
{
   const auto odfDuration = odf.size() / odfSampleRate;
   constexpr std::array<int, 6> numBars { 1, 2, 3, 4, 6, 8 };

   const auto peakIndices = GetOnsetIndices(odf);
   if (peakIndices.empty())
      return .5; // Worst score

   const auto peakSum = std::accumulate(
      peakIndices.begin(), peakIndices.end(), 0.,
      [&](double sum, int i) { return sum + odf[i]; });

   enum class TimeSignature
   {
      FourFour,
      FourFourSwing,
      ThreeFour,
      SixEight,
      SixEightSwung,
      _count
   };

   // iterate through all time signatures:
   constexpr std::array<TimeSignature, static_cast<int>(TimeSignature::_count)>
      timeSignatures { TimeSignature::FourFour, TimeSignature::FourFourSwing,
                       TimeSignature::ThreeFour, TimeSignature::SixEight,
                       TimeSignature::SixEightSwung };

   // Maps a combination of time-signature and number of bars (encoded as
   // string) to a score.
   std::map<std::string, std::pair<double /*score*/, int /*lag*/>> scores;

   // We iterate through all time signatures, and for each, in the inner loop,
   // we will determine a sensible set of number of bars.
   std::for_each(
      timeSignatures.begin(), timeSignatures.end(), [&](TimeSignature ts) {
         const std::vector<bool> pattern =
            ts == TimeSignature::FourFour ?
               // `true` eight times:
               std::vector<bool>(8, true) :
               ts == TimeSignature::FourFourSwing ?
                  // `true, false, true` four times:
                  std::vector<bool> { true, false, true, true, false, true,
                                      true, false, true, true, false, true } :
                  ts == TimeSignature::ThreeFour ? // `true` three times:
                     std::vector<bool>(3, true) :
                     ts == TimeSignature::SixEight ? // `true` six times:
                        std::vector<bool>(6, true) :
                        // `true, false, true` six times:
                        std::vector<bool> { true,  false, true,  true,
                                            false, true,  true,  false,
                                            true,  true,  false, true };
         // TODO Use the pattern (for swung rhythms).
         const auto numDivsPerBar = pattern.size();
         constexpr auto minNumDivsPerMinute = 100;
         constexpr auto maxNumDivsPerMinute = 700;
         const int minNumBars = std::max<double>(
            odfDuration * minNumDivsPerMinute / numDivsPerBar / 60, 1);
         const int maxNumBars =
            odfDuration * maxNumDivsPerMinute / numDivsPerBar / 60;
         std::vector<double> subScores(maxNumBars - minNumBars + 1);
         auto numBars = minNumBars;
         std::for_each(
            subScores.begin(), subScores.end(), [&](double& subScore) {
               const auto numDivs = numBars * numDivsPerBar;
               const auto odfSamplesPerDiv = 1. * odf.size() / numDivs;

               // We will auto-correlate the odf with a pulse train with
               // frequency `numDivs`, to compensate for possible lag (not using
               // for-loop)
               std::vector<float> pulseTrain(odf.size());
               std::fill(pulseTrain.begin(), pulseTrain.end(), 0.f);
               for (auto i = 0; i < numDivs; ++i)
                  pulseTrain[static_cast<int>(i * odfSamplesPerDiv + .5)] = 1.f;

               // There may be a tiny lag between the pulse train and the odf.
               // Take the inner product until we've reached the first peak:
               auto lag = 0;
               auto max = std::numeric_limits<float>::lowest();
               for (auto i = 0; i < static_cast<int>(odfSamplesPerDiv); ++i)
               {
                  const auto p = std::inner_product(
                     odf.begin(), odf.end(), pulseTrain.begin(), 0.f);
                  if (p >= max)
                  {
                     max = p;
                     lag = i;
                  }
                  else
                     break;
                  std::rotate(
                     pulseTrain.begin(),
                     pulseTrain.begin() + pulseTrain.size() - 1,
                     pulseTrain.end());
               }
               std::vector<int> shiftedPeakIndices(peakIndices.size());
               std::transform(
                  peakIndices.begin(), peakIndices.end(),
                  shiftedPeakIndices.begin(),
                  [&](int peakIndex) { return peakIndex - lag; });

               // Get the distance of all peaks to the closest div:
               std::vector<double> peakDistances(peakIndices.size());
               std::transform(
                  shiftedPeakIndices.begin(), shiftedPeakIndices.end(),
                  peakDistances.begin(), [&](int peakIndex) {
                     const auto closestDiv =
                        std::round(peakIndex / odfSamplesPerDiv);
                     // Normalized distance between 0 and 1:
                     const auto distance =
                        (peakIndex - closestDiv * odfSamplesPerDiv) /
                        odfSamplesPerDiv;
                     return distance < 0 ? -distance : distance;
                  });
               // Calculate the score as the sum of the distances weighted by
               // the odf values:
               const auto score =
                  std::inner_product(
                     peakDistances.begin(), peakDistances.end(),
                     peakIndices.begin(), 0.,
                     [](double a, double b) { return a + b; },
                     [&](double a, int i) { return a * odf[i]; }) /
                  peakSum;

               const std::string key =
                  std::to_string(numBars) + "x" + std::to_string(numDivsPerBar);
               scores[key] = { score, lag };

               ++numBars;
               return score;
            });
      });

   // Look at the amplitude across all scores. If it's too small, then it
   // probably isn't something with a definite rhythm.
   const auto maxScoreIt = std::max_element(
      scores.begin(), scores.end(), [](const auto& a, const auto& b) {
         return a.second.first < b.second.first;
      });
   const auto minScoreIt = std::min_element(
      scores.begin(), scores.end(), [](const auto& a, const auto& b) {
         return a.second.first < b.second.first;
      });
   const auto amplitude = maxScoreIt->second.first - minScoreIt->second.first;

   // The errors range from 0 to 0.5. At the moment we set the range of the
   // amplitude weight from 0.5 to 1, but we could consider scaling it to the
   // [0, 1] range.
   return minScoreIt->second.first * (1 - amplitude);
}

std::optional<Key> GetKey(const MirAudioSource& source)
{
   const auto sampleRate = source.GetSampleRate();
   VampPluginConfig config;
   const auto roundedNumSamples =
      1 << static_cast<int>(std::floor(std::log2(source.GetNumSamples())));
   config.blockSize = roundedNumSamples;
   config.stepSize = roundedNumSamples;
   const auto features =
      GetVampFeatures("qmvampplugins:qm-keydetector", source, config);
   if (features.empty())
      return {};
   // TODO
   return {};
}
} // namespace MIR
