/*  SPDX-License-Identifier: GPL-2.0-or-later */
/*!********************************************************************

  Audacity: A Digital Audio Editor

  MusicInformationRetrieval.h

  Matthieu Hodgkinson

**********************************************************************/
#pragma once

#include <cassert>
#include <numeric>
#include <optional>
#include <string>
#include <vector>

namespace MIR
{

class MirAudioSource;
struct BeatInfo;

// Score threshold above which an `MirAudioSource` is *not* considered a loop. (Left public for testing.)
static constexpr auto rhythmicClassifierScoreThreshold = 0.16;

static constexpr auto smoothingThreshold = 1.;

struct ODF
{
   std::vector<double> values;
   const double duration;
};

/*!
 * Information needed to time-synchronize the audio file with the project.
 */
struct ProjectSyncInfo
{
   /*!
    * The tempo of the raw audio file, in quarter-notes per minute.
    */
   const double rawAudioTempo;

   /*!
    * Should be 1 most of the time, but may be 0.5 or 2 to reduce the amount
    * of stretching needed to match the project tempo.
    */
   const double recommendedStretchFactor;

   /*!
    * It is common that loops fill up a bit more than the intended number of
    * bars. If this is detected, this value is written here and may be used for
    * trimming.
    */
   const double excessDurationInQuarternotes;

   const double offsetInQuarternotes;
};

struct Key
{
   const int pitchClass;
   const bool isMajor;
};

class MUSIC_INFORMATION_RETRIEVAL_API MusicInformation
{
public:
   /**
    * @brief Construct a new Music Information object
    * @detail For now we only exploit the filename and duration ...
    */
   MusicInformation(
      const std::string& filename, double duration,
      const MirAudioSource& source);

   const std::string filename;
   const double duration;

   /*!
    * @brief Tells whether the file contains music content.
    */
   operator bool() const;

   /**
    * @brief Get the information needed to synchronize the corresponding file
    * with the project it belongs to.
    * @pre Music content was detected, i.e., `*this == true`
    */
   ProjectSyncInfo
   GetProjectSyncInfo(const std::optional<double>& projectTempo) const;

private:
   /*!
    * For now we either detect constant tempo or no tempo. We may need to
    * extend it to a `map<double , double >` when we have a master track with
    * tempo automation.
    * Note that BPM isn't quarter-notes per minute (QPM), which is the value we
    * need to adjust project tempo. For this, a `timeSignature` is also needed.
    * Else, the most likely QPM can be guessed, but there's always a risk of
    * over- or underestimating by a factor of two.
    */
   const std::optional<double> mBpm;

   /*!
    * In seconds
    */
   const std::optional<double> mOffset;

   // Additional information (time signature(s), key(s), genre, etc) to be added
   // here.
};

// Used internally by `MusicInformation`, made public for testing.
MUSIC_INFORMATION_RETRIEVAL_API std::optional<double>
GetBpmFromFilename(const std::string& filename);

struct ClassifierTuningThresholds
{
   const double isRhythmic;
   const double hasConstantTempo;
};

MUSIC_INFORMATION_RETRIEVAL_API void GetBpmAndOffset(
   double rmsNormalizedOdfXcorrStd, const BeatInfo& beatInfo,
   std::optional<double>& bpm, std::optional<double>& offset,
   std::optional<ClassifierTuningThresholds> tuningThresholds = {});

MUSIC_INFORMATION_RETRIEVAL_API std::optional<double>
GetBpm(const MirAudioSource& source);

MUSIC_INFORMATION_RETRIEVAL_API double GetBeatFittingErrorRms(
   const std::pair<double, double>& coefs, const BeatInfo& info);

MUSIC_INFORMATION_RETRIEVAL_API bool IsRhythmic(
   double rmsNormalizedOdfXcorrStd, std::optional<double> tuningThreshold);

MUSIC_INFORMATION_RETRIEVAL_API bool HasConstantTempo(
   double beatFittingErrorRms, std::optional<double> tuningThreshold);

MUSIC_INFORMATION_RETRIEVAL_API double
GetNormalizedAutocorrCurvatureRms(const std::vector<double>& x);

MUSIC_INFORMATION_RETRIEVAL_API double GetBeatSnr(
   const std::vector<double>& odf, double odfSampleRate,
   const std::vector<double>& beatTimes);

MUSIC_INFORMATION_RETRIEVAL_API std::optional<std::vector<size_t>>
GetBeatIndices(const ODF& odf, const std::vector<float>& xcorr);

MUSIC_INFORMATION_RETRIEVAL_API bool IsLoop(
   const ODF& odf, const std::vector<float>& xcorr,
   const std::vector<size_t>& beatIndices);

MUSIC_INFORMATION_RETRIEVAL_API std::vector<float>
GetNormalizedAutocorrelation(const std::vector<double>& x, bool full = true);

MUSIC_INFORMATION_RETRIEVAL_API std::vector<float>
GetNormalizedAutocorrelation(const std::vector<float>& x, bool full = true);

MUSIC_INFORMATION_RETRIEVAL_API std::pair<double, double>
GetApproximateGcd(const std::vector<float>& odf, double odfSampleRate);

MUSIC_INFORMATION_RETRIEVAL_API std::vector<float> GetOnsetDetectionFunction(
   const MirAudioSource& source, double& odfSampleRate,
   double smoothingThreshold);

MUSIC_INFORMATION_RETRIEVAL_API void NewStuff(const MirAudioSource& source);

struct Experiment1Result
{
   const double score;
   const double tatumRate;
   const double bpm;
};

MUSIC_INFORMATION_RETRIEVAL_API
Experiment1Result Experiment1(
   const std::vector<float>& odf, double odfSampleRate,
   double audioFileDuration);

MUSIC_INFORMATION_RETRIEVAL_API std::optional<Key>
GetKey(const MirAudioSource& source);

template <typename T, typename U>
std::pair<double, double> LinearFit(
   const std::vector<T>& x, const std::vector<U>& y, std::vector<double> w)
{
   assert(x.size() == y.size() && (w.empty() || w.size() == y.size()));
   if (w.empty())
      w = std::vector<double>(y.size(), 1.0);

   // Calculate weighted means
   const double w_mean_x =
      std::inner_product(x.begin(), x.end(), w.begin(), 0.0) /
      std::accumulate(w.begin(), w.end(), 0.0);
   const double w_mean_y =
      std::inner_product(y.begin(), y.end(), w.begin(), 0.0) /
      std::accumulate(w.begin(), w.end(), 0.0);

   auto n = 0;
   const double numerator = std::inner_product(
      x.begin(), x.end(), y.begin(), 0.0, std::plus<>(),
      [&](double xi, double yi) {
         return w[n++] * (xi - w_mean_x) * (yi - w_mean_y);
      });

   n = 0;
   const double denominator = std::inner_product(
      x.begin(), x.end(), x.begin(), 0.0, std::plus<>(),
      [&](double xi, double) {
         return w[n++] * (xi - w_mean_x) * (xi - w_mean_x);
      });

   // Calculate slope (a) and intercept (b)
   const double a = numerator / denominator;
   const double b = w_mean_y - a * w_mean_x;

   return std::make_pair(a, b);
}

} // namespace MIR
