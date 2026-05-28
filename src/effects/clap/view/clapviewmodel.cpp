/*
 * Audacity: A Digital Audio Editor
 */
#include "clapviewmodel.h"

#include "au3-components/EffectInterface.h"

using namespace au::effects;

ClapViewModel::ClapViewModel(QObject* parent, int instanceId)
    : AbstractEffectViewModel(parent, instanceId)
{
}

void ClapViewModel::doInit()
{
    // Nothing host-side: the plugin's native UI manages itself.
}

void ClapViewModel::doStartPreview()
{
    auto access = instancesRegister()->settingsAccessById(m_instanceId);
    if (!access) {
        return;
    }
    access->ModifySettings([this](EffectSettings& settings) {
        executionScenario()->previewEffect(m_instanceId, settings);
        return nullptr;
    });
}

void ClapViewModel::doStopPreview()
{
    executionScenario()->stopPreview();
}
