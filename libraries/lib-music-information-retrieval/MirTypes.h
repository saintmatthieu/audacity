#pragma once

#include <map>
#include <optional>
#include <string>
#include <vector>

namespace MIR
{
struct BeatInfo
{
   std::vector<double> beatTimes;
   std::optional<int> indexOfFirstBeat;
};

struct VampPluginConfig
{
   std::optional<int> blockSize;
   std::optional<int> stepSize;
   std::map<std::string, float> parameters;
};
} // namespace MIR
