/*
 * Audacity: A Digital Audio Editor
 */
#include "realtimeeffectlistitemmodel.h"

namespace au::projectscene {
RealtimeEffectListItemModel::RealtimeEffectListItemModel(QObject* parent, effects::EffectStateId effectStateId)
    : QObject{parent}, effectStateId{effectStateId}
{
    realtimeEffectService()->realtimeEffectActiveChanged().onReceive(this, [this](effects::EffectStateId stateId) {
        if (stateId == this->effectStateId) {
            emit accentButtonChanged();
        }
    });
}

bool RealtimeEffectListItemModel::prop_accentButton() const
{
    return realtimeEffectService()->isRealtimeEffectActive(effectStateId);
}

void RealtimeEffectListItemModel::prop_setAccentButton(bool active)
{
    realtimeEffectService()->setIsRealtimeEffectActive(effectStateId, active);
}

QString RealtimeEffectListItemModel::effectName() const
{
    return QString::fromStdString(effectsProvider()->effectName(*reinterpret_cast<effects::RealtimeEffectState*>(effectStateId)));
}

void RealtimeEffectListItemModel::showDialog()
{
    effectsProvider()->showEffect(reinterpret_cast<effects::RealtimeEffectState*>(effectStateId));
}
}
