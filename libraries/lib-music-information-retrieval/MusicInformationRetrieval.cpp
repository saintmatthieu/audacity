/*  SPDX-License-Identifier: GPL-2.0-or-later */
/*!********************************************************************

  Audacity: A Digital Audio Editor

  MusicInformationRetrieval.cpp

  Matthieu Hodgkinson

**********************************************************************/
#include "MusicInformationRetrieval.h"
#include "GetMeterUsingTatumQuantizationFit.h"
#include "MirAudioReader.h"
#include "MirUtils.h"
#include "StftFrameProvider.h"

#include <array>
#include <cassert>
#include <cmath>
#include <numeric>
#include <regex>

namespace MIR
{
namespace
{
// Normal distribution parameters obtained by fitting a gaussian in the GTZAN
// dataset tempo values.
static constexpr auto bpmExpectedValue = 126.3333;

constexpr auto numTimeSignatures = static_cast<int>(TimeSignature::_count);

auto RemovePathPrefix(const std::string& filename)
{
   return filename.substr(filename.find_last_of("/\\") + 1);
}

// When we get time-signature estimate, we may need a map for that, since 6/8
// has 1.5 quarter notes per beat.
constexpr std::array<double, numTimeSignatures> quarternotesPerBeat { 1., 1.,
                                                                      1.5 };

std::optional<MusicalMeter> FillMetadata(
   const std::string& filename, const MirAudioReader& audio,
   FalsePositiveTolerance tolerance,
   const std::function<void(double progress)>& progressCallback)
{
   const auto bpmFromFilename = GetBpmFromFilename(filename);
   if (bpmFromFilename.has_value())
      return MusicalMeter { *bpmFromFilename, {} };
   else
      return GetMusicalMeterFromSignal(audio, tolerance, progressCallback);
}
} // namespace

int GetNumerator(TimeSignature ts)
{
   constexpr std::array<int, numTimeSignatures> numerators = { 4, 3, 6 };
   return numerators[static_cast<int>(ts)];
}

int GetDenominator(TimeSignature ts)
{
   constexpr std::array<int, numTimeSignatures> denominators = { 4, 4, 8 };
   return denominators[static_cast<int>(ts)];
}

MusicInformation::MusicInformation(
   const std::string& filename, double duration, const MirAudioReader& audio,
   FalsePositiveTolerance tolerance,
   std::function<void(double progress)> progressCallback)
    : filename { RemovePathPrefix(filename) }
    , duration { duration }
    , mMusicalMeter { FillMetadata(
         filename, audio, tolerance, std::move(progressCallback)) }
{
}

MusicInformation::operator bool() const
{
   // For now suffices to say that we have detected music content if there is
   // rhythm.
   return mMusicalMeter.has_value();
}

ProjectSyncInfo MusicInformation::GetProjectSyncInfo(
   const std::optional<double>& projectTempo) const
{
   assert(*this);
   if (!*this)
      return {};

   const auto error = mMusicalMeter->bpm - bpmExpectedValue;

   const auto qpm =
      mMusicalMeter->bpm *
      quarternotesPerBeat[static_cast<int>(
         mMusicalMeter->timeSignature.value_or(TimeSignature::FourFour))];

   auto recommendedStretch = 1.0;
   if (projectTempo.has_value())
      recommendedStretch =
         std::pow(2., std::round(std::log2(*projectTempo / qpm)));

   auto excessDurationInQuarternotes = 0.;
   auto numQuarters = duration * qpm / 60.;
   const auto roundedNumQuarters = std::round(numQuarters);
   const auto delta = numQuarters - roundedNumQuarters;
   // If there is an excess less than a 32nd, we treat it as an edit error.
   if (0 < delta && delta < 1. / 8)
      excessDurationInQuarternotes = delta;

   return { qpm, mMusicalMeter->timeSignature, recommendedStretch,
            excessDurationInQuarternotes };
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

std::optional<MusicalMeter> GetMusicalMeterFromSignal(
   const MirAudioReader& audio, FalsePositiveTolerance tolerance,
   const std::function<void(double)>& progressCallback,
   QuantizationFitDebugOutput* debugOutput)
{
   if (audio.GetNumSamples() / audio.GetSampleRate() > 60)
      // A file longer than 1 minute is most likely not a loop, and processing
      // it would be costly.
      return {};
   return GetMeterUsingTatumQuantizationFit(
      audio, tolerance, progressCallback, debugOutput);
}
} // namespace MIR
