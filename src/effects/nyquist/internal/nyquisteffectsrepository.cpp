/*
* Audacity: A Digital Audio Editor
*/
#include "nyquisteffectsrepository.h"

#include "effects/effects_base/internal/effectsutils.h"
#include "effects/effects_base/effectstypes.h"
#include "au3wrap/internal/wxtypes_convert.h"

#include "au3-module-manager/PluginManager.h"
#include "au3-effects/EffectManager.h"
#include "spectrogram/spectrogramtypes.h"

au::effects::NyquistEffectsRepository::NyquistEffectsRepository(const muse::modularity::ContextPtr& ctx,
                                                                std::unique_ptr<muse::audioplugins::IAudioPluginsScanner> nyquistPluginScanner,
                                                                std::shared_ptr<NyquistPluginsMetaReader> nyquistPluginMetaReader)
    : muse::Injectable(ctx), m_nyquistPluginScanner{std::move(nyquistPluginScanner)}, m_nyquistPluginMetaReader{std::move(
                                                                                                                    nyquistPluginMetaReader)}
{
}

void au::effects::NyquistEffectsRepository::init()
{
    const CompleteAudioResourceMetaList resources = getCompleteAudioResourceMetaList();
    registerPlugins(resources);
    registerSpectralEffects(resources);
}

void au::effects::NyquistEffectsRepository::registerPlugins(const CompleteAudioResourceMetaList& resources) const
{
    std::vector<muse::audioplugins::AudioPluginInfo> infos;
    std::for_each(resources.begin(), resources.end(), [&infos](const auto& pair) {
        const auto& path = pair.first;
        muse::audioplugins::AudioPluginInfo info;
        info.path = path;
        info.meta = utils::toMuseAudioResourceMeta(pair.second);
        info.enabled = true;     // If it already is registered, the register will ignore it anyway, so this value will be preserved if false.
        info.type = muse::audioplugins::AudioPluginType::Fx;
        info.errorCode = 0;
        infos.push_back(std::move(info));
    });

    constexpr auto continueOnDuplicate = true;
    knownAudioPluginsRegister()->registerPlugins(infos, continueOnDuplicate);
}

void au::effects::NyquistEffectsRepository::registerSpectralEffects(const CompleteAudioResourceMetaList& resources) const
{
    for (const auto& meta : effectMetaList(resources)) {
        if (meta.category == utils::builtinEffectCategoryIdString(BuiltinEffectCategoryId::SpectralTools)) {
            const NyquistBase* const nyquistEffect
                = dynamic_cast<const NyquistBase*>(EffectManager::Get().GetEffect(au3::wxFromString(meta.id)));
            if (!nyquistEffect) {
                continue;
            }
            std::optional<spectrogram::SpectralEffectId> spectralEffectIdOpt;
            const auto spectralEffectId = nyquistEffect->GetSpectralEffectId();
            if (spectralEffectId == "DeleteSelection") {
                spectralEffectIdOpt = spectrogram::SpectralEffectId::DeleteSelection;
            } else if (spectralEffectId == "DeleteCenterFrequency") {
                spectralEffectIdOpt = spectrogram::SpectralEffectId::DeleteCenterFrequency;
            } else if (spectralEffectId == "AmplifySelection") {
                spectralEffectIdOpt = spectrogram::SpectralEffectId::AmplifySelection;
            } else if (spectralEffectId == "AmplifyCenterFrequency") {
                spectralEffectIdOpt = spectrogram::SpectralEffectId::AmplifyCenterFrequency;
            }
            if (spectralEffectIdOpt) {
                spectrogram::SpectralEffect spectralEffect;
                spectralEffect.spectralEffectId = *spectralEffectIdOpt;
                spectralEffect.action = effects::makeEffectAction(effects::EFFECT_OPEN_ACTION, meta.id);
                spectralEffect.title = meta.title;
                spectralEffectsRegister()->registerSpectralEffect(spectralEffect);
            }
        }
    }
}

au::effects::EffectMetaList au::effects::NyquistEffectsRepository::effectMetaList(const CompleteAudioResourceMetaList& completeList) const
{
    au::effects::EffectMetaList effects;
    std::transform(completeList.begin(), completeList.end(), std::back_inserter(effects), [](const auto& pair) {
        return pair.second;
    });
    return effects;
}

au::effects::NyquistEffectsRepository::CompleteAudioResourceMetaList au::effects::NyquistEffectsRepository::getCompleteAudioResourceMetaList()
const
{
    CompleteAudioResourceMetaList result;

    for (const muse::io::path_t& path : m_nyquistPluginScanner->scanPlugins()) {
        const std::optional<EffectMeta> meta = m_nyquistPluginMetaReader->readMeta(path);
        if (meta.has_value()) {
            result[path] = *meta;
        } else {
            LOGW() << "Failed to read plugin meta for " << path;
            continue;
        }
    }

    return result;
}

au::effects::EffectMetaList au::effects::NyquistEffectsRepository::effectMetaList() const
{
    const CompleteAudioResourceMetaList resourceMetaList = getCompleteAudioResourceMetaList();
    return effectMetaList(resourceMetaList);
}

bool au::effects::NyquistEffectsRepository::ensurePluginIsLoaded(const EffectId& effectId) const
{
    if (!PluginManager::Get().IsPluginLoaded(au3::wxFromString(effectId))) {
        return PluginManager::Get().Load(au3::wxFromString(effectId)) != nullptr;
    }
    return true;
}
