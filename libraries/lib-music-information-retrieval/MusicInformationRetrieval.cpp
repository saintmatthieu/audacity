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

#include <cassert>
#include <cmath>
#include <fstream>
#include <numeric>
#include <regex>

namespace MIR
{
namespace
{
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
std::vector<size_t> GetOnsetIndices(const std::vector<T>& odf)
{
   // Find indices of all peaks in `odf` whose value is larger than the average.
   const auto avg =
      std::accumulate(odf.begin(), odf.end(), 0) / static_cast<T>(odf.size());
   std::vector<size_t> peakIndices;
   for (auto i = 1; i < odf.size() - 1; ++i)
      if (odf[i - 1] <= odf[i] && odf[i] >= odf[i + 1] && odf[i] > avg)
         peakIndices.push_back(i);
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
   // Regex: If we have a succession of 2 or 3 digits preceeded by either
   // underscore or dash, optionally followed by (case-insensitive) "bpm", and
   // then either dash, underscore or dot, we have a match.
   const std::regex bpmRegex { R"((?:_|-)(\d{2,3})(bpm)?(_|-|\.))",
                               std::regex::icase };

   // Now there may be several matches, as in "Hipness_808_fonky3_89.wav" -
   // let's find them all.
   std::smatch matches;
   std::string::const_iterator searchStart(filename.cbegin());
   std::vector<int> bpmCandidates;
   while (std::regex_search(searchStart, filename.cend(), matches, bpmRegex))
   {
      try
      {
         const auto bpm = std::stoi(matches[1]);
         if (30 < bpm && bpm < 220)
            bpmCandidates.push_back(bpm);
      }
      catch (const std::invalid_argument& e)
      {
         assert(false);
      }
      searchStart = matches.suffix().first;
   }

   const auto bestMatch = std::min_element(
      bpmCandidates.begin(), bpmCandidates.end(),
      [](int a, int b) { return std::abs(a - 120) < std::abs(b - 120); });
   if (bestMatch != bpmCandidates.end())
   {
      return *bestMatch;
   }
   else
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

std::vector<float> GetNormalizedAutocorrelation(const std::vector<double>& x)
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
   const auto normalizer = 1 / xcorr[0];
   std::transform(
      xcorr.begin(), xcorr.end(), xcorr.begin(),
      [normalizer](float x) { return x * normalizer; });
   return xcorr;
}

std::vector<float> GetNormalizedAutocorrelation(const std::vector<float>& x)
{
   std::vector<double> xDouble(x.begin(), x.end());
   return GetNormalizedAutocorrelation(xDouble);
}

std::pair<double, double>
GetApproximateGcd(const std::vector<float>& odf, double odfSampleRate)
{
   const auto peakIndices = GetOnsetIndices(odf);
   std::vector<std::pair<double /*time*/, double /*weights*/>> onsets(
      peakIndices.size());
   std::transform(
      peakIndices.begin(), peakIndices.end(), onsets.begin(), [&](size_t i) {
         return std::pair<double, double> { i / odfSampleRate, odf[i] };
      });
   // Normalize `onsets` so that they sum up to 1:
   const auto normalizer =
      1. /
      std::accumulate(onsets.begin(), onsets.end(), 0., [](double a, auto b) {
         return a + b.second;
      });
   std::for_each(
      onsets.begin(), onsets.end(), [&](auto& x) { x.second *= normalizer; });

   // `onsets` was derived from audio which may be a musical loop or
   // not. We will therefore return our best guess together with a confidence
   // value.
   // IF the audio is a loop, we expect the first beat to be at time 0, hence we
   // don't expect an offset in the onset times.
   // We still have to try and find the approximate GCD. We may use
   // `onsets` as relative weights.

   // Just doing it brute force, iterating through all BPM values from 60 to
   // 600.
   constexpr auto firstBpm = 60;
   std::vector<int> bpmCandidates(600 - firstBpm + 1);
   std::iota(bpmCandidates.begin(), bpmCandidates.end(), firstBpm);
   std::vector<double> errorRms(bpmCandidates.size());
   std::transform(
      bpmCandidates.begin(), bpmCandidates.end(), errorRms.begin(),
      [&](int bpm) {
         const auto period = 60. / bpm;
         std::vector<double> errors(onsets.size());
         std::transform(
            onsets.begin(), onsets.end(), errors.begin(),
            [&](const std::pair<double, double>& onset) {
               const auto [t, weight] = onset;
               const auto closestBeatIndex = std::round(t / period);
               const auto distance = t - closestBeatIndex * period;
               return distance * distance * weight;
            });
         return std::accumulate(errors.begin(), errors.end(), 0.);
      });
   const auto winner =
      std::distance(
         errorRms.begin(), std::min_element(errorRms.begin(), errorRms.end()));
   return { bpmCandidates[winner], errorRms[winner] };
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
} // namespace

std::vector<float>
GetOnsetDetectionFunction(const MirAudioSource& source, double& odfSampleRate)
{
   const auto sampleRate = source.GetSampleRate();
   const auto [hopSize, frameSize] = GetHopAndFrameSizes(sampleRate);
   long long start = 0;
   std::vector<float> buffer(frameSize);
   std::vector<float> odf;
   const auto powSpecSize = frameSize / 2 + 1;
   std::vector<float> prevPowSpec(powSpecSize);
   std::fill(prevPowSpec.begin(), prevPowSpec.end(), 0.f);

   std::vector<float> hann(frameSize);
   auto n = 0;
   std::transform(hann.begin(), hann.end(), hann.begin(), [&](float x) {
      return 0.5f * (1 - std::cos(2 * 3.141592653589793 * n++ / frameSize));
   });

   std::ofstream ofsStft("C:/Users/saint/Downloads/log_stft.txt");

   while (source.ReadFloats(buffer.data(), start, frameSize) == frameSize)
   {
      std::transform(
         buffer.begin(), buffer.end(), hann.begin(), buffer.begin(),
         [](float a, float b) { return a * b; });
      PowerSpectrum(frameSize, buffer.data(), buffer.data());
      // Compress the frame as per Fundamentals of Music Processing, (6.5)
      constexpr auto gamma = 100.;
      std::transform(
         buffer.begin(), buffer.begin() + powSpecSize, buffer.begin(),
         [](float x) { return std::log(1 + gamma * std::sqrt(x)); });
      auto k = 0;
      const auto sum = std::accumulate(
         buffer.begin(), buffer.begin() + powSpecSize, 0.,
         [&](float a, float mag) {
            // Half-wave-rectified stuff
            return a + std::max(0.f, mag - prevPowSpec[k++]);
         });

      std::for_each(buffer.begin(), buffer.begin() + powSpecSize, [&](float x) {
         ofsStft << x << ",";
      });
      ofsStft << std::endl;

      std::copy(
         buffer.begin(), buffer.begin() + powSpecSize, prevPowSpec.begin());
      odf.push_back(sum);
      start += hopSize;
   }

   std::ofstream ofsRawOdf("C:/Users/saint/Downloads/log_raw.txt");
   std::for_each(
      odf.begin(), odf.end(), [&](float x) { ofsRawOdf << x << ","; });
   ofsRawOdf << std::endl;

   constexpr auto idealM = 20;
   const auto M = std::min(idealM, static_cast<int>(odf.size()) / idealM + 1);
   n = 0;
   std::vector<double> movingAverage(odf.size());
   std::transform(odf.begin(), odf.end(), movingAverage.begin(), [&](float x) {
      auto sum = 0.;
      for (auto i = 0; i < M; ++i)
         sum += odf[(n + i) % odf.size()];
      ++n;
      return sum / (2 * M + 1);
   });

   std::ofstream ofsAvg("C:/Users/saint/Downloads/log_avg.txt");
   std::for_each(movingAverage.begin(), movingAverage.end(), [&](double x) {
      ofsAvg << x << ",";
   });
   ofsAvg << std::endl;

   constexpr auto smooth = false;
   if (smooth)
      // subtract moving average from odf:
      std::transform(
         odf.begin(), odf.end(), movingAverage.begin(), odf.begin(),
         [](float a, float b) { return std::max(a - b, 0.f); });

   odfSampleRate = static_cast<double>(sampleRate) / hopSize;
   return odf;
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
