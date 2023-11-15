/*  SPDX-License-Identifier: GPL-2.0-or-later */
/*!********************************************************************

  Audacity: A Digital Audio Editor

  MusicInformationRetrieval.cpp

  Matthieu Hodgkinson

**********************************************************************/
#include "MusicInformationRetrieval.h"

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

MusicInformation::MusicInformation(const std::string& filename, double duration)
    : filename { RemovePathPrefix(filename) }
    , duration { duration }
    , mBpm { GetBpmFromFilename(filename) }
{
   // A file's title may claim a BPM of e.g. 120. Based on this and the
   // duration, we get a number of beats. If we detect that this number is very
   // close to being round, then we probably have a loop.

   // A perfectly edited loop would actually yield an exactly round number. In
   // practice, though, this is often not the case. In this situation, either

   // 1. we don't do anything, leaving the tempo untouched. But then if the user
   // repeats the loop by copy-pasting, a drift will build up. Or

   // 2. we assume an exact number of beats, thus modifying slightly the tempo
   // to probably a fractional value.

   // Solution 2. "distributes" the error across the loop duration, which for
   // decently edited loops shouldn't be perceivable. Drift introduced by 1 may
   // very well become perceivable, though. So let's go for 2.
   AdjustTempoToExactNumBeatsIfLoop();
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

   return { qpm, recommendedStretch };
}

void MusicInformation::AdjustTempoToExactNumBeatsIfLoop()
{
   if (!mBpm.has_value())
      return;
   auto numbeats = duration * *mBpm / 60.;
   const auto roundedNumbeats = std::round(numbeats);
   if (std::abs(numbeats - roundedNumbeats) < 0.1)
      mBpm = 60. * roundedNumbeats / duration;
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
} // namespace MIR
