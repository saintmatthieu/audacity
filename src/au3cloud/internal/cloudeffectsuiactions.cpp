/*
* Audacity: A Digital Audio Editor
*/
#include "cloudeffectsuiactions.h"

#include "context/shortcutcontext.h"
#include "context/uicontext.h"

using namespace au::au3cloud;

const muse::ui::UiActionList& CloudEffectsUiActions::actionsList() const
{
    if (m_actions.empty()) {
        for (const CloudEffectItem& e : provider()->effects()) {
            m_actions.push_back(muse::ui::UiAction(cloudEffectOpenActionCode(e.id),
                                                   au::context::UiCtxAny,
                                                   au::context::CTX_ANY,
                                                   e.title,
                                                   e.title));
        }
    }

    return m_actions;
}

bool CloudEffectsUiActions::actionEnabled(const muse::ui::UiAction&) const
{
    return true;
}

bool CloudEffectsUiActions::actionChecked(const muse::ui::UiAction&) const
{
    return false;
}

muse::async::Channel<muse::actions::ActionCodeList> CloudEffectsUiActions::actionEnabledChanged() const
{
    return m_actionEnabledChanged;
}

muse::async::Channel<muse::actions::ActionCodeList> CloudEffectsUiActions::actionCheckedChanged() const
{
    return m_actionCheckedChanged;
}
