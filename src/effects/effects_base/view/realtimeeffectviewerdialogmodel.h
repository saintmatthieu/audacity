/*
 * Audacity: A Digital Audio Editor
 */
#pragma once

#include "modularity/ioc.h"
#include "ieffectinstancesregister.h"
#include "ieffectsprovider.h"
#include "irealtimeeffectservice.h"
#include "effectstypes.h"
#include "async/asyncable.h"

#include <QObject>

namespace au::effects {
class RealtimeEffectViewerDialogModel : public QObject, public muse::Injectable, public muse::async::Asyncable
{
    Q_OBJECT
    Q_PROPERTY(QString effectState READ prop_effectState WRITE prop_setEffectState FINAL)
    Q_PROPERTY(bool accentButton READ prop_accentButton WRITE prop_setAccentButton NOTIFY accentButtonChanged FINAL)

    muse::Inject<IEffectInstancesRegister> instancesRegister;
    muse::Inject<IEffectsProvider> effectsProvider;
    muse::Inject<IRealtimeEffectService> realtimeEffectService;

public:
    RealtimeEffectViewerDialogModel(QObject* parent = nullptr);
    ~RealtimeEffectViewerDialogModel() override;

    QString prop_effectState() const;
    void prop_setEffectState(const QString& effectState);
    bool prop_accentButton() const;
    void prop_setAccentButton(bool accentButton);

signals:
    void accentButtonChanged();

private:
    EffectStateId effectStateId() const;
    void unregisterState();

    RealtimeEffectStatePtr m_effectState;
};
}
