/*  SPDX-License-Identifier: GPL-2.0-or-later */
/*!********************************************************************

  Audacity: A Digital Audio Editor

  MirTypes.h

  Matthieu Hodgkinson

**********************************************************************/
#pragma once

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


struct PffftFloatVectorAllocator : public std::allocator<float>
{
   float* allocate(std::size_t n);
   void deallocate(float* const p, std::size_t n);
};

using PffftFloatVector = std::vector<float, PffftFloatVectorAllocator>;
} // namespace MIR
