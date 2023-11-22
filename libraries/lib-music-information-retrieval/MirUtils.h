#pragma once

#include <optional>
#include <utility>

namespace MIR
{
class MirAudioSource;
struct BeatInfo;

std::optional<std::pair<double, double>>
GetBeatFittingCoefficients(const BeatInfo& source);
} // namespace MIR
