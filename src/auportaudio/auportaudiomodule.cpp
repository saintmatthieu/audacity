/*
 * Audacity: A Digital Audio Editor
 */
#include "auportaudiomodule.h"

#include "framework/global/modularity/ioc.h"

#include "audio/driver/audiodrivercontroller.h"

#include "au3-audio-devices/DeviceManager.h"

#include "internal/portaudiodevice.h"
#include "internal/portaudiodevices.h"

using namespace au::auportaudio;

namespace {
const std::string mname("auportaudio");

class PortAudioDriver final : public IPortAudioDriver
{
public:
    explicit PortAudioDriver(std::shared_ptr<au::audio::AudioDriverController> driverController)
        : m_driverController{std::move(driverController)} {}

    std::shared_ptr<au::audio::AudioDriverController> driverController() const override { return m_driverController; }
    std::shared_ptr<au::audio::IDeviceOpener> deviceOpener() const override { return m_deviceOpener; }

private:
    const std::shared_ptr<au::audio::AudioDriverController> m_driverController;
    const std::shared_ptr<au::audio::IDeviceOpener> m_deviceOpener = std::make_shared<PortAudioDeviceOpener>();
};
}

std::string AuPortAudioModule::moduleName() const
{
    return mname;
}

void AuPortAudioModule::registerExports()
{
    m_driverController = std::make_shared<audio::AudioDriverController>(std::make_unique<PortAudioDevices>(), "AudioIO");
    m_driver = std::make_shared<PortAudioDriver>(m_driverController);
    globalIoc()->registerExport<IPortAudioDriver>(mname, m_driver);
}

void AuPortAudioModule::onInit(const muse::IApplication::RunMode&)
{
    audio::AudioDriverController::initSharedDefaults(DeviceManager::GetOptimalSupportedSampleRate());
    m_driverController->init();
}
