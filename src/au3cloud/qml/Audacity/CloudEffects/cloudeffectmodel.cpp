/*
* Audacity: A Digital Audio Editor
*/
#include "cloudeffectmodel.h"

#include "log.h"

using namespace au::au3cloud;

CloudEffectModel::CloudEffectModel(QObject* parent)
    : QObject(parent)
{
}

void CloudEffectModel::apply(const QString& effectId, const QVariant& params)
{
    LOGI() << "cloud effect apply, effectId: " << effectId << ", params count: " << params.toMap().size();

    if (toastService()) {
        toastService()->showInfo("Cloud effect initiated...", effectId.toStdString());
    }
}
