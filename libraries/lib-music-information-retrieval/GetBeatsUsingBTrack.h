#pragma once

#include "MirTypes.h"

namespace MIR
{
class MirAudioSource;

namespace GetBeatsUsingBTrack
{
std::optional<BeatInfo> GetBeats(const MirAudioSource& source);
}
} // namespace MIR
