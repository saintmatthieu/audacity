#pragma once

#include <vector>

namespace ClipAnalysis
{
struct ODF
{
   std::vector<double> values;
   const double duration;
};

enum class TimeSignature
{
   FourFour,
   ThreeFour,
   SixEight,
   _Count,
};
} // namespace ClipAnalysis
