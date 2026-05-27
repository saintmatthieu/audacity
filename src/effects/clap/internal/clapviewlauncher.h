/*
* Audacity: A Digital Audio Editor
*/
#pragma once

#include "effects/effects_base/internal/abstractviewlauncher.h"

namespace au::effects {
class ClapViewLauncher final : public AbstractViewLauncher
{
public:
    ClapViewLauncher(const muse::modularity::ContextPtr& ctx)
        : AbstractViewLauncher(ctx) {}

    muse::Ret showEffect(const EffectInstanceId& instanceId) const override;
    void showRealtimeEffect(const RealtimeEffectStatePtr& state) const override;
};
}
