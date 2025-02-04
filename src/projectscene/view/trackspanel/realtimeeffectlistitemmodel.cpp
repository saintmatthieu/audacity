/*
 * Audacity: A Digital Audio Editor
 */
#include "realtimeeffectlistitemmodel.h"
#include "log.h"

namespace au::projectscene {
RealtimeEffectListItemModel::RealtimeEffectListItemModel(QObject* parent, effects::RealtimeEffectStatePtr effectStateId)
    : QObject{parent}, effectStateId{std::move(effectStateId)}
{
    realtimeEffectService()->isActiveChanged().onReceive(this, [this](effects::RealtimeEffectStatePtr stateId)
    {
        if (stateId == this->effectStateId) {
            emit isActiveChanged();
        }
    });
}

RealtimeEffectListItemModel::~RealtimeEffectListItemModel()
{
    effectsProvider()->hideEffect(effectStateId);
}

int RealtimeEffectListItemModel::getIndex() const
{
    const auto index = realtimeEffectService()->effectIndex(effectStateId);
    if (!index.has_value()) {
        return 0;
    }
    return *index;
}

bool RealtimeEffectListItemModel::prop_isMasterEffect() const
{
    return realtimeEffectService()->trackId(effectStateId) == effects::IRealtimeEffectService::masterTrackId;
}

QString RealtimeEffectListItemModel::effectName() const
{
    return QString::fromStdString(effectsProvider()->effectName(*effectStateId));
}

void RealtimeEffectListItemModel::toggleDialog()
{
    effectsProvider()->toggleShowEffect(effectStateId);
}

bool RealtimeEffectListItemModel::prop_isActive() const
{
    return realtimeEffectService()->isActive(effectStateId);
}

void RealtimeEffectListItemModel::prop_setIsActive(bool isActive)
{
    realtimeEffectService()->setIsActive(effectStateId, isActive);
}
}
