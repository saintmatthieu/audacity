/*
 * Audacity: A Digital Audio Editor
 */
#include "realtimeeffectviewerdialogmodel.h"
#include "libraries/lib-realtime-effects/RealtimeEffectState.h"
#include "libraries/lib-effects/EffectPlugin.h"
#include "log.h"

namespace au::effects {
RealtimeEffectViewerDialogModel::RealtimeEffectViewerDialogModel(QObject* parent)
    : QObject(parent)
{
    realtimeEffectService()->realtimeEffectActiveChanged().onReceive(this, [this](EffectStateId stateId) {
        if (stateId == effectStateId()) {
            return;
        }
        emit accentButtonChanged();
    });
}

RealtimeEffectViewerDialogModel::~RealtimeEffectViewerDialogModel()
{
    realtimeEffectService()->realtimeEffectActiveChanged().resetOnReceive(this);
    unregisterState();
}

QString RealtimeEffectViewerDialogModel::prop_effectState() const
{
    if (!m_effectState) {
        return {};
    }
    return QString::number(effectStateId());
}

void RealtimeEffectViewerDialogModel::prop_setEffectState(const QString& effectState)
{
    if (effectState == prop_effectState()) {
        return;
    }

    unregisterState();

    if (effectState.isEmpty()) {
        return;
    }

    m_effectState = reinterpret_cast<RealtimeEffectState*>(effectState.toULongLong())->shared_from_this();
    const auto effectId = m_effectState->GetID().ToStdString();
    const auto type = effectsProvider()->effectSymbol(effectId);
    const auto instance = std::dynamic_pointer_cast<effects::EffectInstance>(m_effectState->GetInstance());
    instancesRegister()->regInstance(muse::String::fromStdString(effectId), instance, m_effectState->GetAccess());
}

void RealtimeEffectViewerDialogModel::unregisterState()
{
    if (!m_effectState) {
        return;
    }

    const auto instance = std::dynamic_pointer_cast<effects::EffectInstance>(m_effectState->GetInstance());
    instancesRegister()->unregInstance(instance);
    m_effectState.reset();
}

bool RealtimeEffectViewerDialogModel::prop_accentButton() const
{
    return realtimeEffectService()->isRealtimeEffectActive(effectStateId());
}

void RealtimeEffectViewerDialogModel::prop_setAccentButton(bool accentButton)
{
    realtimeEffectService()->setIsRealtimeEffectActive(effectStateId(), accentButton);
}

EffectStateId RealtimeEffectViewerDialogModel::effectStateId() const
{
    IF_ASSERT_FAILED(m_effectState) {
        return 0;
    }

    return reinterpret_cast<uintptr_t>(m_effectState.get());
}
}
