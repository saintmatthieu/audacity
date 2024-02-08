/*  SPDX-License-Identifier: GPL-2.0-or-later */
/*!********************************************************************

  Audacity: A Digital Audio Editor

  MirTypes.h

  Matthieu Hodgkinson

**********************************************************************/
#pragma once

#include <array>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace MIR
{
enum class FalsePositiveTolerance
{
   Strict,
   Lenient,
};

enum class TimeSignature
{
   TwoTwo,
   FourFour,
   ThreeFour,
   SixEight,
   _count
};

inline int GetNumerator(TimeSignature ts)
{
   constexpr std::array<int, static_cast<int>(TimeSignature::_count)>
      numerators = { 2, 4, 3, 6 };
   return numerators[static_cast<int>(ts)];
}

inline int GetDenominator(TimeSignature ts)
{
   constexpr std::array<int, static_cast<int>(TimeSignature::_count)>
      denominators = { 2, 4, 4, 8 };
   return denominators[static_cast<int>(ts)];
}

/*!
 * @brief How the tempo was obtained:
 * - looking for RIFF and ACID metadata in a WAV file's header,
 * - looking for a tempo in the title of the file,
 * - analyzing the signal.
 */
enum class TempoObtainedFrom
{
   Header,
   Title,
   Signal,
};

struct MusicalMeter
{
   const double bpm;
   const std::optional<TimeSignature> timeSignature;
};

struct OnsetQuantization
{
   double error = 0.;
   int lag = 0;
   int numDivisions = 0;
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
    * The method used to obtain the tempo.
    */
   const TempoObtainedFrom usedMethod;

   /*!
    * The time-signature of the raw audio file.
    */
   const std::optional<TimeSignature> timeSignature;

   /*!
    * Should be 1 most of the time, but may be 0.5 or 2 to reduce the amount
    * of stretching needed to match the project tempo.
    */
   const double stretchMinimizingPowOfTwo = 1.;

   /*!
    * It is common that loops fill up a bit more than the intended number of
    * bars. If this is detected, this value is written here and may be used for
    * trimming.
    */
   const double excessDurationInQuarternotes = 0.;
};

class MirAudioReader
{
public:
   virtual double GetSampleRate() const = 0;
   virtual long long GetNumSamples() const = 0;
   virtual void
   ReadFloats(float* buffer, long long where, size_t numFrames) const = 0;
   double GetDuration() const
   {
      return GetSampleRate() == 0 ? 0. : GetNumSamples() / GetSampleRate();
   }
   virtual ~MirAudioReader() = default;
};

class AnalyzedAudioClip
{
public:
   virtual const std::optional<MIR::ProjectSyncInfo>& GetSyncInfo() const = 0;
   virtual void SetRawAudioTempo(double tempo) = 0;
   virtual void Synchronize() = 0;
   virtual ~AnalyzedAudioClip() = default;
};

struct QuantizationFitDebugOutput
{
   OnsetQuantization tatumQuantization;
   double bpm = 0;
   std::optional<TimeSignature> timeSignature;
   double score = 0.;
   std::vector<std::vector<float>> postProcessedStft;
   std::vector<float> rawOdf;
   std::vector<float> movingAverage;
   std::vector<float> odf;
   double odfSr = 0.;
   double audioFileDuration = 0.;
   std::vector<int> odfPeakIndices;
   std::vector<float> odfAutoCorr;
   std::vector<int> odfAutoCorrPeakIndices;
};

// Some internal constants centralized here, to stress their interdependence.

/*!
 * Frequency energy compression factor. See section (6.5) in Müller, Meinard.
 * Fundamentals of music processing: Audio, analysis, algorithms,
 * applications. Vol. 5. Cham: Springer, 2015.
 */
static constexpr auto gamma = 100.f;

/*!
 * Target hop size in seconds for the onset detection function. Due to the
 * power-of-two length constraint of the ODF, the actual value will be slightly
 * different.
 */
static constexpr auto targetHopDuration = 0.01f;

/*!
 * At 44.1kHz, the frame size is 2048, which corresponds to 46ms. This duration
 * works, and should be (approximately) preserved across other sample rates.
 */
static constexpr auto frameSizeExponentAt44_1kHz = 11;

/*!
 * In our implementation, the ODF of a steady sinusoid isn't exactly zero. This
 * threshold is here to prevent the onset detection function from triggering on
 * such noise.
 * Careful: modifying `gamma` or `targetHopDuration` might require re-tuning
 * this threshold.
 */
static constexpr auto odfNoiseGate = 5e-4f;

} // namespace MIR
