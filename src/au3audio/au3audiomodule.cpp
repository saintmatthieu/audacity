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
#include "internal/au4/au4audioengine.h"
#include "internal/portaudio/portaudiodevice.h"

#ifdef AU_USE_JUCE
#include "internal/juce/juceaudiodevices.h"
#include "internal/juce/jucedevice.h"
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
    // Over AudioIO's devices and settings
    m_portAudioEngine = std::make_shared<au4::Au4AudioEngine>(std::make_shared<au4::PortAudioDeviceOpener>());
    m_portAudioEngine->setDriverController(m_audioDriverController.get());

#ifdef AU_USE_JUCE
    const auto juceDevices = std::make_shared<au4::JuceAudioDevices>();
    m_juceEngine = std::make_shared<au4::Au4AudioEngine>(std::make_shared<au4::JuceDeviceOpener>(juceDevices));
    m_juceDriverController = std::make_shared<AudioDriverController>(juceDevices, "JuceAudio");
    m_juceDriverController->setAudioEngine(m_juceEngine);
    m_juceEngine->setDriverController(m_juceDriverController.get());
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
    // AudioIO and an Au4AudioEngine share the PortAudio devices: the driver controller follows the active one
    const auto suspendPortAudioStream = [this] { return m_audioDriverController->suspendStream(); };
    backendRegister->registerBackend({ "portaudio", m_audioEngine, m_audioDriverController, suspendPortAudioStream,
                                       "PortAudio (AudioIO)", [this] { m_audioDriverController->setAudioEngine(m_audioEngine); } });
    backendRegister->registerBackend({ "portaudio-safe", m_portAudioEngine, m_audioDriverController, suspendPortAudioStream,
                                       "PortAudio (safe)", [this] {
            m_audioDriverController->setAudioEngine(m_portAudioEngine);
            m_portAudioEngine->setMode(au4::Au4AudioEngine::Mode::Safe);
        } });
    backendRegister->registerBackend({ "portaudio-fast", m_portAudioEngine, m_audioDriverController, suspendPortAudioStream,
                                       "PortAudio (fast)", [this] {
            m_audioDriverController->setAudioEngine(m_portAudioEngine);
            m_portAudioEngine->setMode(au4::Au4AudioEngine::Mode::Fast);
        } });
#ifdef AU_USE_JUCE
    // One engine, which plays as the active backend says
    const auto suspendJuceStream = [this] { return m_juceDriverController->suspendStream(); };
    backendRegister->registerBackend({ "juce", m_juceEngine, m_juceDriverController, suspendJuceStream, "JUCE (safe)",
                                       [this] { m_juceEngine->setMode(au4::Au4AudioEngine::Mode::Safe); } });
    backendRegister->registerBackend({ "juce-fast", m_juceEngine, m_juceDriverController, suspendJuceStream, "JUCE (fast)",
                                       [this] { m_juceEngine->setMode(au4::Au4AudioEngine::Mode::Fast); } });
#endif
}

void Au3AudioModule::onInit(const muse::IApplication::RunMode&)
{
    m_audioEngine->init();
    AudioDriverController::initSharedDefaults(AudioIOBase::GetOptimalSupportedSampleRate());
    m_audioDriverController->init();
#ifdef AU_USE_JUCE
    m_juceDriverController->init();
#endif
    m_systemAudioDevicesListener->startListening();
}

void Au3AudioModule::onDeinit()
{
    m_systemAudioDevicesListener->stopListening();
    m_audioEngine->deinit();
}
