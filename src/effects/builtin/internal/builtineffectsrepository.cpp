/*
* Audacity: A Digital Audio Editor
*/
#include "builtineffectsrepository.h"

#include <QtQml>

#include "global/translation.h"

#include "libraries/lib-module-manager/PluginManager.h"

#include "libraries/lib-effects/LoadEffects.h"

#include "au3wrap/internal/wxtypes_convert.h"

#include "general/generalviewmodel.h"

#include "amplify/amplifyeffect.h"
#include "amplify/amplifyviewmodel.h"
#include "tonegen/chirpeffect.h"
#include "tonegen/toneeffect.h"
#include "reverb/reverbeffect.h"
#include "reverb/reverbviewmodel.h"
#include "tonegen/toneviewmodel.h"

#include "log.h"

#include <algorithm>

using namespace au::effects;

void BuiltinEffectsRepository::preInit()
{
    static BuiltinEffectsModule::Registration< AmplifyEffect > regAmplify;
    static BuiltinEffectsModule::Registration< ChirpEffect > regChirp;
    static BuiltinEffectsModule::Registration< ToneEffect > regTone;
    static BuiltinEffectsModule::Registration< ReverbEffect > regReverb;
}

void BuiltinEffectsRepository::init()
{
    updateEffectMetaList();
}

void BuiltinEffectsRepository::updateEffectMetaList()
{
    // For now, this method is called only once, so there is yet no need to clear the list and unregister the views.
    // It will have to be implemented when we provide the user the possibility of rescanning the effects, though.
    assert(m_metas.empty());

    auto regView = [this](const ::ComponentInterfaceSymbol& symbol, const muse::String& url) {
        effectsViewRegister()->regUrl(au3::wxToString(symbol.Internal()), url);
    };

    auto regMeta = [this](const ::PluginDescriptor& desc, const muse::String& title, const muse::String& description) {
        EffectMeta meta;
        meta.id = au3::wxToString(desc.GetID());
        meta.categoryId = BUILTIN_CATEGORY_ID;
        meta.title = title;
        meta.description = description;
        meta.isRealtimeCapable = desc.IsEffectRealtime();
        m_metas.insert({ desc.GetSymbol(), meta });
    };

    // General
    qmlRegisterUncreatableType<AbstractEffectModel>("Audacity.Effects", 1, 0, "AbstractEffectModel", "Not creatable abstract type");
    qmlRegisterType<GeneralViewModel>("Audacity.Effects", 1, 0, "GeneralViewModel");
    effectsViewRegister()->setDefaultUrl(u"qrc:/general/GeneralEffectView.qml");

    for (const PluginDescriptor& desc : PluginManager::Get().PluginsOfType(PluginTypeEffect)) {
        const auto& symbol = desc.GetSymbol();
        if (symbol == AmplifyEffect::Symbol) {
            qmlRegisterType<AmplifyViewModel>("Audacity.Effects", 1, 0, "AmplifyViewModel");
            regView(AmplifyEffect::Symbol, u"qrc:/amplify/AmplifyView.qml");
            regMeta(desc,
                    muse::mtrc("effects", "Amplify"),
                    muse::mtrc("effects", "Increases or decreases the volume of the audio you have selected")
                    );
        } else if (symbol == ChirpEffect::Symbol) {
            regView(ChirpEffect::Symbol, u"qrc:/tonegen/ChirpView.qml");
            regMeta(desc,
                    muse::mtrc("effects", "Chirp"),
                    muse::mtrc("effects", "Generates an ascending or descending tone of one of four types")
                    );
        } else if (symbol == ToneEffect::Symbol) {
            qmlRegisterType<ToneViewModel>("Audacity.Effects", 1, 0, "ToneViewModel");
            regView(ToneEffect::Symbol, u"qrc:/tonegen/ToneView.qml");
            regMeta(desc,
                    muse::mtrc("effects", "Tone"),
                    muse::mtrc("effects", "Generates a constant frequency tone of one of four types")
                    );
        } else if (symbol == ReverbEffect::Symbol) {
            qmlRegisterType<ReverbViewModel>("Audacity.Effects", 1, 0, "ReverbViewModel");
            regView(ReverbEffect::Symbol, u"qrc:/reverb/ReverbView.qml");
            regMeta(desc,
                    muse::mtrc("effects", "Reverb"),
                    muse::mtrc("effects", "Reverb effect")
                    );
        } else {
            LOGW() << "effect not found for symbol: " << au3::wxToStdSting(symbol.Internal());
        }
    }

    m_effectMetaListUpdated.notify();
}

muse::async::Notification BuiltinEffectsRepository::effectMetaListUpdated() const
{
    return m_effectMetaListUpdated;
}

EffectMeta BuiltinEffectsRepository::effectMeta(const ComponentInterfaceSymbol& symbol) const
{
    auto it = m_metas.find(symbol);
    if (it == m_metas.end()) {
        LOGW() << "not found effect meta for symbol: " << au3::wxToStdSting(symbol.Internal());
        return EffectMeta();
    }
    return it->second;
}

EffectMetaList BuiltinEffectsRepository::effectMetaList() const
{
    EffectMetaList list(m_metas.size());
    std::transform(m_metas.begin(), m_metas.end(), list.begin(), [](const auto& pair) { return pair.second; });
    return list;
}
