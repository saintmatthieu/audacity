/*
 * Audacity: A Digital Audio Editor
 */
#pragma once

#include "global/types/string.h"
#include <optional>

namespace au::audio {
using EffectState = void*;

using EffectId = muse::String;

using TrackId = long;

using EffectChainLinkIndex = int;

struct EffectChainLink {
    EffectChainLink(std::string effectName, EffectState effectState)
        : effectName{std::move(effectName)}, effectState{effectState} {}
    const std::string effectName;
    const EffectState effectState;
};

using EffectChainLinkPtr = std::shared_ptr<EffectChainLink>;
}
