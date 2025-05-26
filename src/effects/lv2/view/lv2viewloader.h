/*
 * Audacity: A Digital Audio Editor
 */
#pragma once

#include <QObject>
#include <QQmlComponent>
#include <QQuickItem>

#include "global/async/asyncable.h"

#include "modularity/ioc.h"
#include "effects/effects_base/ieffectsviewregister.h"
#include "effects/effects_base/ieffectsuiengine.h"
#include "effects/effects_base/ieffectinstancesregister.h"

namespace au::effects {
//! TODO Move to builtin module
class Lv2ViewLoader : public QObject, public muse::async::Asyncable
{
    Q_OBJECT

    Q_PROPERTY(QQuickItem * contentItem READ contentItem NOTIFY contentItemChanged FINAL)

    muse::Inject<IEffectsViewRegister> viewRegister;
    muse::Inject<IEffectsUiEngine> engine;
    muse::Inject<IEffectInstancesRegister> instancesRegister;

public:
    Lv2ViewLoader(QObject* parent = nullptr);
    ~Lv2ViewLoader() override;

    QQuickItem* contentItem() const;

    Q_INVOKABLE void load(const QString& instanceId, QObject* itemParent);

signals:
    void titleChanged();
    void contentItemChanged();

    void closeRequested();

private:
    QQuickItem* m_contentItem = nullptr;
};
}
