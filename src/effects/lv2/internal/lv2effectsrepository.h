/*
 * Audacity: A Digital Audio Editor
 */
#pragma once

#include "../ilv2effectsrepository.h"

#include "modularity/ioc.h"
#include "audioplugins/iknownaudiopluginsregister.h"

namespace au::effects {
class Lv2EffectsRepository : public ILv2EffectsRepository
{
    muse::Inject<muse::audioplugins::IKnownAudioPluginsRegister> knownPlugins;

public:
    Lv2EffectsRepository() = default;

    EffectMetaList effectMetaList() const override;
    bool ensurePluginIsLoaded(const EffectId& effectId) const override;
};
}
