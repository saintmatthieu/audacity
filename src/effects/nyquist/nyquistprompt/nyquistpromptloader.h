/*
 * Audacity: A Digital Audio Editor
 */
#pragma once

#include "framework/global/modularity/ioc.h"
#include "effects/builtin/ibuiltineffectsviewregister.h"
#include "effects/builtin/ibuiltineffectsrepository.h"

#include "au3-utility/Observer.h"

class WaveChannel;

namespace au::effects {
class NyquistPromptLoader : public muse::Injectable, public IBuiltinEffectsLoader
{
    muse::GlobalInject<IBuiltinEffectsViewRegister> builtinEffectsViewRegister;

public:

    NyquistPromptLoader(const muse::modularity::ContextPtr& ctx)
        : muse::Injectable(ctx) {}

    static void preInit();

    void init();
    EffectMetaList effectMetaList() const override;
    muse::async::Notification effectMetaListUpdated() const override { return m_effectMetaListUpdated; }

private:
    muse::async::Notification m_effectMetaListUpdated;
    ::Observer::Subscription m_pluginManagerSubscription;
};
}
