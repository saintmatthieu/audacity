/*
* Audacity: A Digital Audio Editor
*/
#pragma once

#include "au3-module-manager/PluginDescriptor.h"
#include "modularity/ioc.h"

#include "effects/builtin/ibuiltineffectsviewregister.h"
#include "effects/builtin/ibuiltineffectsrepository.h"
#include "modularity/ioc.h"

#include "au3-utility/Observer.h"

class ComponentInterfaceSymbol;

namespace au::effects {
class BuiltinEffectsLoader : public muse::Injectable, public IBuiltinEffectsLoader
{
    muse::GlobalInject<IBuiltinEffectsViewRegister> builtinEffectsViewRegister;

public:
    BuiltinEffectsLoader(const muse::modularity::ContextPtr& ctx)
        : muse::Injectable(ctx) {}

    static void preInit();
    void init();

private:
    EffectMetaList effectMetaList() const override;
    muse::async::Notification effectMetaListUpdated() const override { return m_effectMetaListUpdated; }
    void regView(const ::ComponentInterfaceSymbol& symbol, const muse::String& url) const;

    muse::async::Notification m_effectMetaListUpdated;
    ::Observer::Subscription m_pluginManagerSubscription;
};
}
