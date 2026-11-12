/*
 * Audacity: A Digital Audio Editor
 */
#pragma once

#include <memory>

#include "framework/global/modularity/imoduleinterface.h"

namespace au::audio {
class AudioDriverController;
class IDeviceOpener;
}

namespace au::auportaudio {
//! The PortAudio devices, which the audio engines opening them share
class IPortAudioDriver : MODULE_GLOBAL_INTERFACE
{
    INTERFACE_ID(IPortAudioDriver)

public:
    virtual ~IPortAudioDriver() = default;

    //! Of the PortAudio devices, with the settings of AudioIO
    virtual std::shared_ptr<audio::AudioDriverController> driverController() const = 0;
    //! Opens PortAudio streams as AudioIO does, on the devices of its settings
    virtual std::shared_ptr<audio::IDeviceOpener> deviceOpener() const = 0;
};
}
