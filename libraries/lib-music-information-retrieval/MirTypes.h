#pragma once

#include <optional>
#include <vector>

namespace MIR
{
struct BeatInfo
{
   std::vector<double> beatTimes;
   std::optional<int> indexOfFirstBeat;
};
} // namespace MIR
