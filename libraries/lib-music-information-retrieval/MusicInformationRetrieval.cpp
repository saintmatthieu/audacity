/*  SPDX-License-Identifier: GPL-2.0-or-later */
/*!********************************************************************

  Audacity: A Digital Audio Editor

  MusicInformationRetrieval.cpp

  Matthieu Hodgkinson

**********************************************************************/
#include "MusicInformationRetrieval.h"
#include "GetBeats.h"
#include "MirAudioSource.h"

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

void FillMetadata(
   const std::string& filename, const MirAudioSource& source,
   std::optional<double>& bpm, std::optional<double>& offset)
{
   const auto bpmFromFilename = GetBpmFromFilename(filename);
   if (bpmFromFilename.has_value())
      bpm = bpmFromFilename;
   else if (const auto coefs = GetBeatFittingCoefficients(source))
   {
      bpm = 60. / coefs->first;
      offset = -coefs->second;
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

std::optional<std::pair<double, double>>
GetBeatFittingCoefficients(const MirAudioSource& source)
{
   const auto info =
      GetBeatInfo(BeatTrackingAlgorithm::QeenMaryBarBeatTrack, source);
   if (!info.has_value() || info->beatTimes.size() < 2)
      return {};

   // Fit a model which assumes constant tempo, and hence a beat time `t_k` at
   // `alpha*(k0+k) + beta`, in least-square sense, where `k0` is the index of
   // the first beat, which we know, and `k \in [0, N)`. That is, find `beta`
   // and `alpha` such that `sum_k (t_k - alpha*(k0+k) - beta)^2` is minimized.
   // In matrix form, this is
   // | k0     1 | | alpha | = | t_0     |
   // | k0+1   1 | | beta  |   | t_1     |
   // | k0+2   1 |             | t_2     |
   // |  ...     |             | ...     |
   // | k0+N-1 1 |             | t_(N-1) |
   // i.e, Ax = b
   // x = (A^T A)^-1 A^T b
   // A^T A = | X Y |
   //         | Y Z |
   // where X = N, Y = N*k0 + N*(N-1)/2, Z = N*(N-1)*(2N-1)/6
   // where X = N, Y = N*(N-1)/2, Z = N*(N-1)*(2N-1)/6
   // and the inverse - call it M - is
   // (A^T A)^-1 = | Z -Y | / d = M / d
   //              | -Y X |
   // where d = XZ - Y^2
   // Now M A^T = | Z  Z-Y  Z-2Y ... Z-(N-1)Y | = W
   //             | -Y X-Y  2X-Y ... (N-1)X-Y |

   // Get index of first beat, which is readily available from the beat tracking
   // algorithm:
   const auto k0 = info->indexOfFirstBeat.value_or(0);
   const auto N = info->beatTimes.size();
   const auto X =
      N * k0 * k0 + k0 * N * (N - 1) + N * (N - 1) * (2 * N - 1) / 6.;
   const auto Y = N * k0 + N * (N - 1) / 2.;
   const auto Z = static_cast<double>(N);
   const auto d = X * Z - Y * Y;

   static_assert(std::is_same_v<decltype(Y), const double>);
   static_assert(std::is_same_v<decltype(X), const double>);

   std::vector<int> n(N);
   std::iota(n.begin(), n.end(), 0);
   std::vector<double> W0(N);
   std::vector<double> W1(N);
   std::transform(n.begin(), n.end(), W0.begin(), [Z, Y, k0](int k) {
      return Z * (k0 + k) - Y;
   });
   std::transform(n.begin(), n.end(), W1.begin(), [X, Y, k0](int k) {
      return X - Y * (k0 + k);
   });
   const auto alpha =
      std::inner_product(W0.begin(), W0.end(), info->beatTimes.begin(), 0.) / d;
   const auto beta =
      std::inner_product(W1.begin(), W1.end(), info->beatTimes.begin(), 0.) / d;

   return { { alpha, beta } };
}
} // namespace MIR
