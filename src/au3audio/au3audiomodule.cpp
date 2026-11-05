/*
* Audacity: A Digital Audio Editor
*/
#include "au3audiomodule.h"

#include "framework/global/log.h"
#include "framework/global/modularity/ioc.h"

#include "audio/iaudiobackendregister.h"

#include "internal/au3audioengine.h"
#include "internal/audiodrivercontroller.h"
#include "internal/portaudiodevices.h"

#ifdef AU_USE_TRACKTION
#include "internal/juce/juceaudiodevices.h"
#include "internal/juce/juceaudioengine.h"
#include "internal/tracktion/tracktionplaybackstream.h"
#endif

#include "au3-audio-devices/AudioIOBase.h"

#if defined(Q_OS_MAC)
#include "internal/platform/macos/macossystemaudiodeviceslistener.h"
#elif defined(Q_OS_WIN)
#include "internal/platform/windows/windowssystemaudiodeviceslistener.h"
#elif defined(AU_AU3AUDIO_HAS_UDEV)
#include "internal/platform/linux/linuxsystemaudiodeviceslistener.h"
#else
#include "internal/platform/stub/stubsystemaudiodeviceslistener.h"
#endif

using namespace au::au3audio;
using namespace muse::modularity;

static const std::string mname("au3audio");

std::string Au3AudioModule::moduleName() const
{
    return mname;
}

void Au3AudioModule::registerExports()
{
    m_audioEngine = std::make_shared<Au3AudioEngine>();
    m_audioDriverController = std::make_shared<AudioDriverController>(std::make_unique<PortAudioDevices>(), "AudioIO");
    m_audioDriverController->setAudioEngine(m_audioEngine);

#ifdef AU_USE_TRACKTION
    const auto juceDevices = std::make_shared<JuceAudioDevices>();
    m_tracktionAudioEngine = std::make_shared<JuceAudioEngine>(juceDevices, [](auto&&... args) -> std::unique_ptr<PlaybackStream> {
        return std::make_unique<TracktionPlaybackStream>(std::forward<decltype(args)>(args)...);
    }, "Tracktion");
    m_tracktionDriverController = std::make_shared<AudioDriverController>(juceDevices, "TracktionAudio");
    m_tracktionDriverController->setAudioEngine(m_tracktionAudioEngine);
    m_tracktionAudioEngine->setDriverController(m_tracktionDriverController.get());
#endif

#if defined(Q_OS_MAC)
    m_systemAudioDevicesListener = std::make_shared<MacosSystemAudioDevicesListener>();
#elif defined(Q_OS_WIN)
    m_systemAudioDevicesListener = std::make_shared<WindowsSystemAudioDevicesListener>();
#elif defined(AU_AU3AUDIO_HAS_UDEV)
    m_systemAudioDevicesListener = std::make_shared<LinuxSystemAudioDevicesListener>();
#else
    m_systemAudioDevicesListener = std::make_shared<StubSystemAudioDevicesListener>();
#endif

    globalIoc()->registerExport<ISystemAudioDevicesListener>(mname, m_systemAudioDevicesListener);
}

void Au3AudioModule::resolveImports()
{
    const auto backendRegister = globalIoc()->resolve<audio::IAudioBackendRegister>(mname);
    IF_ASSERT_FAILED(backendRegister) {
        return;
    }
    backendRegister->registerBackend({ "portaudio", m_audioEngine, m_audioDriverController,
                                       [this] { return m_audioDriverController->suspendStream(); } });
#ifdef AU_USE_TRACKTION
    backendRegister->registerBackend({ "tracktion", m_tracktionAudioEngine, m_tracktionDriverController,
                                       [this] { return m_tracktionDriverController->suspendStream(); } });
#endif
}

void Au3AudioModule::onInit(const muse::IApplication::RunMode&)
{
    m_audioEngine->init();
    AudioDriverController::initSharedDefaults(AudioIOBase::GetOptimalSupportedSampleRate());
    m_audioDriverController->init();
#ifdef AU_USE_TRACKTION
    m_tracktionDriverController->init();
#endif
    m_systemAudioDevicesListener->startListening();
}

void Au3AudioModule::onDeinit()
{
    m_systemAudioDevicesListener->stopListening();
    m_audioEngine->deinit();
}
