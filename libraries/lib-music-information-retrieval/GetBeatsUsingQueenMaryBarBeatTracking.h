#pragma once

#include "MirTypes.h"

namespace MIR
{
class MirAudioSource;

namespace GetBeatsUsingQueenMaryBarBeatTracking
{
std::optional<BeatInfo> GetBeats(const MirAudioSource&);
}
} // namespace MIR
