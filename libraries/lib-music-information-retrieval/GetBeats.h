#pragma once

#include "MirAudioSource.h"
#include "MirTypes.h"

#include <memory>
#include <optional>
#include <vector>

namespace MIR
{
enum class BeatTrackingAlgorithm
{
   QeenMaryBarBeatTrack,
};

std::optional<BeatInfo>
GetBeatInfo(BeatTrackingAlgorithm algorithm, const MirAudioSource& source);
} // namespace MIR
