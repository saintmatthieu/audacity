/*
* Audacity: A Digital Audio Editor
*/
#pragma once

#include <memory>

#include "framework/global/modularity/imodulesetup.h"

namespace au::au3audio {
class Au3AudioEngine;
class AudioDriverController;
class JuceAudioEngine;
class ISystemAudioDevicesListener;

class Au3AudioModule : public muse::modularity::IModuleSetup
{
public:

    std::string moduleName() const override;
    void registerExports() override;
    void resolveImports() override;
    void onInit(const muse::IApplication::RunMode& mode) override;
    void onDeinit() override;

private:
    std::shared_ptr<Au3AudioEngine> m_audioEngine;
    std::shared_ptr<AudioDriverController> m_audioDriverController;
#ifdef AU_USE_JUCE
    std::shared_ptr<JuceAudioEngine> m_juceWorkerAudioEngine;
    std::shared_ptr<AudioDriverController> m_juceWorkerDriverController;
#endif
#ifdef AU_USE_TRACKTION
    std::shared_ptr<JuceAudioEngine> m_tracktionAudioEngine;
    std::shared_ptr<AudioDriverController> m_tracktionDriverController;
#endif
    std::shared_ptr<ISystemAudioDevicesListener> m_systemAudioDevicesListener;
};
}
