/*
* Audacity: A Digital Audio Editor
*/
#pragma once

#include "../inyquisteffectsrepository.h"
#include "nyquistpluginsmetareader.h"

#include "spectrogram/ispectraleffectsregister.h"

#include "framework/global/modularity/ioc.h"
#include "framework/audioplugins/iaudiopluginsscanner.h"
#include "framework/audioplugins/iknownaudiopluginsregister.h"

#include "au3-nyquist-effects/LoadNyquist.h"

#include <map>

namespace au::effects {
class NyquistEffectsRepository : public INyquistEffectsRepository, public muse::Injectable
{
    muse::Inject<spectrogram::ISpectralEffectsRegister> spectralEffectsRegister { this };
    muse::GlobalInject<muse::audioplugins::IKnownAudioPluginsRegister> knownAudioPluginsRegister;

public:
    NyquistEffectsRepository(const muse::modularity::ContextPtr& ctx,
                             std::unique_ptr<muse::audioplugins::IAudioPluginsScanner> nyquistPluginScanner,
                             std::shared_ptr<NyquistPluginsMetaReader> nyquistPluginMetaReader);

    void init();

    EffectMetaList effectMetaList() const override;
    bool ensurePluginIsLoaded(const EffectId& effectId) const override;

private:
    using CompleteAudioResourceMetaList = std::map<muse::io::path_t, EffectMeta>;
    CompleteAudioResourceMetaList getCompleteAudioResourceMetaList() const;
    EffectMetaList effectMetaList(const CompleteAudioResourceMetaList&) const;
    void registerPlugins(const CompleteAudioResourceMetaList&) const;
    void registerSpectralEffects(const CompleteAudioResourceMetaList&) const;

    // This member forces the linker to include LoadNyquist.cpp,
    // which contains DECLARE_BUILTIN_PROVIDER for the Nyquist module
    ::NyquistEffectsModule m_module;
    const std::unique_ptr<muse::audioplugins::IAudioPluginsScanner> m_nyquistPluginScanner;
    const std::shared_ptr<NyquistPluginsMetaReader> m_nyquistPluginMetaReader;
};
}
