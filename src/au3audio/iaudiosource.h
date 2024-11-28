/*
 * Audacity: A Digital Audio Editor
 */
#pragma once

#include <memory>

namespace au::audio {
class IAudioSource
{
public:
    virtual ~IAudioSource() = default;
    virtual int numChannels() const = 0;
};

using IAudioSourcePtr = std::shared_ptr<IAudioSource>;
using IAudioSourceWPtr = std::weak_ptr<IAudioSource>;
}
