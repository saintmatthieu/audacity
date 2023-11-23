#pragma once

#include <optional>
#include <utility>
#include <vector>

UTILITY_API std::optional<std::pair<double, double>> GetBeatFittingCoefficients(
   const std::vector<double>& beatTimes,
   const std::optional<int>& indexOfFirstBeat = {});
