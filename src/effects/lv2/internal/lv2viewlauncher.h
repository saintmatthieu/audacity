/*
 * Audacity: A Digital Audio Editor
 */
#pragma once

#include "effects/effects_base/internal/abstractviewlauncher.h"

#include "global/modularity/ioc.h"
#include "effects/effects_base/ieffectinstancesregister.h"
#include "global/iinteractive.h"

namespace au::effects {
class Lv2ViewLauncher : public AbstractViewLauncher
{
    // muse::Inject<muse::vst::IVstInstancesRegister> museInstancesRegister;

public:
    Lv2ViewLauncher() = default;

    muse::Ret showEffect(const EffectInstanceId& instanceId) const override;
    void showRealtimeEffect(const RealtimeEffectStatePtr& state) const override;

private:
    void registerFxPlugin(const EffectInstanceId& instanceId) const;
};
}
