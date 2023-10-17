#include "TimeDiv.h"

#pragma once

namespace ClipAnalysis
{
TimeDiv::TimeDiv(
   double score, double cumScore, double maxScore, double varScore)
    : score(score)
    , cumScore(cumScore)
    , maxScore(maxScore)
    , varScore(varScore)
{
   subs.fill(nullptr);
}

TimeDiv::~TimeDiv()
{
   for (auto p = 0; p < numPrimes; ++p)
      if (subs[p])
         delete subs[p];
}
} // namespace ClipAnalysis
