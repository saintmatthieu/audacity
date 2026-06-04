/*
* Audacity: A Digital Audio Editor
*/
#pragma once

#include "au3cloud/icloudeffectsprovider.h"

namespace au::au3cloud {
class CloudEffectsProvider : public ICloudEffectsProvider
{
public:
    const std::vector<CloudEffectItem>& effects() const override;

private:
    static const std::vector<CloudEffectItem> m_effects;
};
}
