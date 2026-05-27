/*
* Audacity: A Digital Audio Editor
*/
#include "clapviewlauncher.h"

using namespace au::effects;

muse::Ret ClapViewLauncher::showEffect(const EffectInstanceId& instanceId) const
{
    return doShowEffect(instanceId, EffectFamily::CLAP);
}

void ClapViewLauncher::showRealtimeEffect(const RealtimeEffectStatePtr& state) const
{
    doShowRealtimeEffect(state);
}
