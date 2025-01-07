/*
 * Audacity: A Digital Audio Editor
 */
#pragma once

#include "modularity/ioc.h"
#include "effects/effects_base/ieffectsprovider.h"
#include "effects/effects_base/irealtimeeffectservice.h"
#include "async/asyncable.h"

#include <QObject>

namespace au::projectscene {
class RealtimeEffectListItemModel : public QObject, public muse::Injectable, muse::async::Asyncable
{
    Q_OBJECT
    Q_PROPERTY(bool accentButton READ prop_accentButton WRITE prop_setAccentButton NOTIFY accentButtonChanged FINAL);

    muse::Inject<effects::IEffectsProvider> effectsProvider;
    muse::Inject<effects::IRealtimeEffectService> realtimeEffectService;

public:
    RealtimeEffectListItemModel(QObject* parent, effects::EffectStateId effectState);

    bool prop_accentButton() const;
    void prop_setAccentButton(bool active);

    const effects::EffectStateId effectStateId;
    Q_INVOKABLE QString effectName() const;
    Q_INVOKABLE void showDialog();

signals:
    void accentButtonChanged();
};
}
