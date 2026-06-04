/*
* Audacity: A Digital Audio Editor
*/
#pragma once

#include "framework/ui/iuiactionsmodule.h"

#include "modularity/ioc.h"
#include "au3cloud/icloudeffectsprovider.h"

namespace au::au3cloud {
//! Registers a Ui action per shipped cloud effect so they can appear in the menu.
class CloudEffectsUiActions : public muse::ui::IUiActionsModule
{
    muse::GlobalInject<ICloudEffectsProvider> provider;

public:
    const muse::ui::UiActionList& actionsList() const override;

    bool actionEnabled(const muse::ui::UiAction& act) const override;
    muse::async::Channel<muse::actions::ActionCodeList> actionEnabledChanged() const override;

    bool actionChecked(const muse::ui::UiAction& act) const override;
    muse::async::Channel<muse::actions::ActionCodeList> actionCheckedChanged() const override;

private:
    mutable muse::ui::UiActionList m_actions;
    muse::async::Channel<muse::actions::ActionCodeList> m_actionEnabledChanged;
    muse::async::Channel<muse::actions::ActionCodeList> m_actionCheckedChanged;
};
}
