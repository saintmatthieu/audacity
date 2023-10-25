#include "TimeDiv.h"

#include <numeric>

namespace ClipAnalysis
{
TimeDiv::TimeDiv(
   double score, double cumScore, double maxScore, double varScore)
    : score(score)
    , cumScore(cumScore)
    , maxScore(maxScore)
    , varScore(varScore)
{
}

TimeDiv::~TimeDiv()
{
   for (auto& p : subs)
      delete p.second;
}

TimeDiv2::TimeDiv2(double mean)
    : mean(mean)
{
}

TimeDiv2::~TimeDiv2()
{
   for (auto& p : subs)
      delete p.second;
}
} // namespace ClipAnalysis
