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

struct TimeDiv2
{
   TimeDiv2(double mean);
   ~TimeDiv2();

   const double mean;
   std::map<int, TimeDiv2*> subs;
};
} // namespace ClipAnalysis
