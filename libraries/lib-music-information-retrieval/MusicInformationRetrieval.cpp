/*  SPDX-License-Identifier: GPL-2.0-or-later */
/*!********************************************************************

  Audacity: A Digital Audio Editor

  MusicInformationRetrieval.cpp

  Matthieu Hodgkinson

**********************************************************************/
#include "MusicInformationRetrieval.h"
#include "FFT.h"
#include "GetBeatFittingCoefficients.h"
#include "GetBeats.h"
#include "GetVampFeatures.h"
#include "MirAudioSource.h"

#include <array>
#include <cassert>
#include <cmath>
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

class ScalingAudioSource : public MirAudioSource
{
public:
   ScalingAudioSource(const MirAudioSource& source, double scalar)
       : mSource { source }
       , mScalar { scalar }
   {
   }

   size_t
   ReadFloats(float* buffer, long long where, size_t numFrames) const override
   {
      const auto numRead = mSource.ReadFloats(buffer, where, numFrames);
      std::transform(buffer, buffer + numRead, buffer, [this](float x) {
         return x * mScalar;
      });
      return numRead;
   }

   int GetSampleRate() const override
   {
      return mSource.GetSampleRate();
   }

   long long GetNumSamples() const override
   {
      return mSource.GetNumSamples();
   }

private:
   const MirAudioSource& mSource;
   const double mScalar;
};

auto GetRms(const MirAudioSource& source)
{
   auto squaredSum = 0.;
   auto start = 0;
   constexpr auto blockSize = 1024;
   std::array<float, blockSize> buffer;
   while (true)
   {
      const auto numRead = source.ReadFloats(buffer.data(), start, blockSize);
      if (numRead == 0)
         break;
      squaredSum += std::reduce(
         buffer.begin(), buffer.begin() + numRead, 0.f,
         [](float a, float b) { return a + b * b; });
      start += blockSize;
   }
   return std::sqrt(squaredSum / source.GetNumSamples());
}

std::vector<float> GetAutocorrelation(const std::vector<double>& x)
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
   return xcorr;
}

bool IsRhythmic(
   const MirAudioSource& source, std::optional<double> tuningThreshold)
{
   // Here is how we proceed:
   // 1. We need to normalize the signal by its RMS.
   // 2. We apply the onset-detection function plugin from Queen Mary.
   // 3. We auto-correlate the onset detection function with itself. More than
   // just percussiveness, we want repetitions at regular intervals to weigh in.
   // 4. We finally take the variance of that.

   // We will then have to tune this threshold in some unit tests.

   // Step 1:
   const auto rms = GetRms(source);
   const ScalingAudioSource scaledSource { source, 1. / rms / 10 };

   // Step 2:
   const auto features =
      GetVampFeatures("qmvampplugins:qm-onsetdetector", scaledSource);
   constexpr auto onsetDetectionFunctionIndex = 1;
   const auto& onsetDetectionFunction =
      features.at(onsetDetectionFunctionIndex);
   std::vector<double> odf(onsetDetectionFunction.size());
   std::transform(
      onsetDetectionFunction.begin(), onsetDetectionFunction.end(), odf.begin(),
      [](const auto& feature) { return feature.values[0]; });

   // Step 3:
   const auto xcorr = GetAutocorrelation(odf);

   // Step 4:
   const auto mean =
      std::accumulate(xcorr.begin(), xcorr.end(), 0.) / xcorr.size();
   const auto stdDev = std::sqrt(
      std::accumulate(
         xcorr.begin(), xcorr.end(), 0.,
         [mean](double a, double b) {
            const auto diff = b - mean;
            return a + diff * diff;
         }) /
      xcorr.size());

   constexpr auto defaultThreshold = 6e+6;
   const auto threshold = tuningThreshold.value_or(defaultThreshold);
   return stdDev > threshold;
}

void GetBpmAndOffsetInternal(
   const BeatInfo& info, std::optional<double>& bpm,
   std::optional<double>& offset, std::optional<double> tuningThreshold)
{
   if (info.beatTimes.size() < 2)
      return;
   const auto [alpha, beta] =
      GetBeatFittingCoefficients(info.beatTimes, info.indexOfFirstBeat);
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
   const auto mean =
      std::accumulate(squaredErrors.begin(), squaredErrors.end(), 0.) /
      static_cast<double>(squaredErrors.size());
   // If the average error is more than something noticeable, i.e., a 16th of a
   // beat (at most a 32nd, more likely a 64th), then don't trust the fit.
   constexpr auto meanBeatErrorThreshold = 1. / 16;
   const auto meanThreshold =
      tuningThreshold.value_or(meanBeatErrorThreshold * meanBeatErrorThreshold);
   if (mean >= meanThreshold)
      return;
   bpm = 60. / alpha;
   offset = -beta;
}

void FillMetadata(
   const std::string& filename, const MirAudioSource& source,
   std::optional<double>& bpm, std::optional<double>& offset)
{
   const auto bpmFromFilename = GetBpmFromFilename(filename);
   if (bpmFromFilename.has_value())
      bpm = bpmFromFilename;
   else
   {
      const auto beatInfo =
         GetBeats(BeatTrackingAlgorithm::QueenMaryBarBeatTrack, source);
      if (beatInfo.has_value())
         GetBpmAndOffset(source, *beatInfo, bpm, offset);
   }
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
   const MirAudioSource& source, const BeatInfo& beatInfo,
   std::optional<double>& bpm, std::optional<double>& offset,
   std::optional<ClassifierTuningThresholds> tuningThresholds)
{
   if (!IsRhythmic(
          source, tuningThresholds.has_value() ?
                     std::optional<double> { tuningThresholds->isRhythmic } :
                     std::nullopt))
      return;
   GetBpmAndOffsetInternal(
      beatInfo, bpm, offset,
      tuningThresholds.has_value() ?
         std::optional<double> { tuningThresholds->hasConstantTempo } :
         std::nullopt);
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
