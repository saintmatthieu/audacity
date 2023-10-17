#pragma once

#include <map>

namespace ClipAnalysis
{
struct TimeDiv
{
   TimeDiv(double score, double cumScore, double maxScore, double varScore);
   ~TimeDiv();

   const double score;
   const double cumScore;
   const double maxScore;
   const double varScore;
   std::map<int, TimeDiv*> subs;
};
} // namespace ClipAnalysis
