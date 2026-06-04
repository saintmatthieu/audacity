/*
* Audacity: A Digital Audio Editor
*/
#include "cloudeffectsprovider.h"

using namespace au::au3cloud;

//! NOTE Prototype: shipped cloud effects are hardcoded here. To add another dummy,
//! add an entry below and a matching <id>.qml (deriving from CloudEffect) to the
//! Audacity.CloudEffects QML module.
const std::vector<CloudEffectItem> CloudEffectsProvider::m_effects = {
    { "dummy",
      muse::TranslatableString("cloudeffects", "Dummy cloud effect"),
      QStringLiteral("qrc:/qt/qml/Audacity/CloudEffects/DummyCloudEffect.qml") }
};

const std::vector<CloudEffectItem>& CloudEffectsProvider::effects() const
{
    return m_effects;
}
