/*
* Audacity: A Digital Audio Editor
*/
#pragma once

#include "framework/actions/actionable.h"
#include "framework/actions/actiontypes.h"
#include "framework/global/async/asyncable.h"

#include "framework/global/modularity/ioc.h"
#include "framework/actions/iactionsdispatcher.h"
#include "framework/interactive/iinteractive.h"
#include "au3cloud/icloudeffectsprovider.h"

namespace au::au3cloud {
//! Registers an "open" action per shipped cloud effect and opens its dialog.
class CloudEffectsActionsController : public muse::actions::Actionable, public muse::async::Asyncable, public muse::Contextable
{
    muse::GlobalInject<ICloudEffectsProvider> provider;

    muse::ContextInject<muse::actions::IActionsDispatcher> dispatcher { this };
    muse::ContextInject<muse::IInteractive> interactive { this };

public:
    CloudEffectsActionsController(const muse::modularity::ContextPtr& ctx)
        : muse::Contextable(ctx) {}

    void init();

private:
    void openCloudEffect(const muse::actions::ActionQuery& q);
};
}
