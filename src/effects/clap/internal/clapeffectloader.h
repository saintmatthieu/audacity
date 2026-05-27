/*
 * Audacity: A Digital Audio Editor
 */
#pragma once

#include "effects/effects_base/internal/au3effectloader.h"

#include "au3-clap/ClapEffectsProvider.h"

namespace au::effects {
class ClapEffectLoader final : public Au3EffectLoader
{
public:
    ClapEffectLoader()
        : Au3EffectLoader(m_module, muse::audio::AudioResourceType::ClapPlugin) {}

private:
    ClapEffectsProvider m_module;
};
} // namespace au::effects
