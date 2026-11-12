/*
 * Audacity: A Digital Audio Editor
 */
#pragma once

#include <memory>

#include "framework/global/modularity/imoduleinterface.h"

namespace au::audio {
class AudioDriverController;
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
};
}
