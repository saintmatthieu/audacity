/*
* Audacity: A Digital Audio Editor
*/
#include "cloudeffectsactionscontroller.h"

#include "log.h"

using namespace au::au3cloud;

void CloudEffectsActionsController::init()
{
    for (const CloudEffectItem& e : provider()->effects()) {
        dispatcher()->reg(this, muse::actions::ActionQuery(cloudEffectOpenActionCode(e.id)),
                          [this](const muse::actions::ActionQuery& q) { openCloudEffect(q); });
    }
}

void CloudEffectsActionsController::openCloudEffect(const muse::actions::ActionQuery& q)
{
    const std::string effectId = q.param("effectId").toString();

    QString qmlUrl;
    for (const CloudEffectItem& e : provider()->effects()) {
        if (e.id == effectId) {
            qmlUrl = e.qmlUrl;
            break;
        }
    }

    if (qmlUrl.isEmpty()) {
        LOGW() << "unknown cloud effect: " << effectId;
        return;
    }

    muse::UriQuery uri(CLOUD_EFFECT_DIALOG_URI);
    uri.addParam("effectUrl", muse::Val(qmlUrl.toStdString()));
    interactive()->openSync(uri);
}
