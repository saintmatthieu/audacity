/*
 * Audacity: A Digital Audio Editor
 */
#pragma once

#include <memory>

#include "framework/global/modularity/imodulesetup.h"

#include "iportaudiodriver.h"

namespace au::auportaudio {
class AuPortAudioModule : public muse::modularity::IModuleSetup
{
public:
    std::string moduleName() const override;
    void registerExports() override;
    void onInit(const muse::IApplication::RunMode& mode) override;

private:
    std::shared_ptr<audio::AudioDriverController> m_driverController;
    std::shared_ptr<IPortAudioDriver> m_driver;
};
}
