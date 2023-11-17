/*  SPDX-License-Identifier: GPL-2.0-or-later */
/*!********************************************************************

  Audacity: A Digital Audio Editor

  MusicInformationRetrieval.cpp

  Matthieu Hodgkinson

**********************************************************************/
#include "MusicInformationRetrieval.h"
#include "FixedTempoEstimator.h"
#include "MirAudioSource.h"

#include <cassert>
#include <cmath>
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
} // namespace

MusicInformation::MusicInformation(
   const std::string& filename, double duration, const MirAudioSource& source)
    : filename { RemovePathPrefix(filename) }
    , duration { duration }
    , mBpm { GetBpmFromFilename(filename) }
{
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

   return { qpm, recommendedStretch, excessDurationInQuarternotes };
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

std::optional<double> GetBpmFromSignal(const MirAudioSource& source)
{
   FixedTempoEstimator estimator(source.GetSampleRate());
   return {};
}
} // namespace MIR
