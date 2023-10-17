#pragma once

#include <array>

namespace ClipAnalysis
{
constexpr auto numPrimes = 4;
constexpr std::array<int, numPrimes> primes { 2, 3, 5, 7 };

struct TimeDiv
{
   TimeDiv(
      double score, double cumScore = 1, double maxScore = 1,
      double varScore = 1);

   ~TimeDiv();

   const double score;
   const double cumScore;
   const double maxScore;
   const double varScore;
   std::array<TimeDiv*, numPrimes> subs;
};
} // namespace ClipAnalysis
