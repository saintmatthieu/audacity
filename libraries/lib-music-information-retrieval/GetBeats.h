#pragma once

#include "MirAudioSource.h"
#include "MirTypes.h"

#include <memory>
#include <optional>
#include <vector>

namespace MIR
{
MUSIC_INFORMATION_RETRIEVAL_API std::optional<BeatInfo>
GetBeats(BeatTrackingAlgorithm algorithm, const MirAudioSource& source);
} // namespace MIR
