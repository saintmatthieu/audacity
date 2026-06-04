/*
* Audacity: A Digital Audio Editor
*/
#pragma once

#include <QObject>
#include <QString>
#include <QVariant>
#include <QtQml/qqmlregistration.h>

#include "modularity/ioc.h"
#include "toast/itoastservice.h"

namespace au::au3cloud {
//! QML-facing model composed by CloudEffect. On apply() it shows a toast and
//! (later) collects the project/selection and calls IAu3AudioComService.
class CloudEffectModel : public QObject
{
    Q_OBJECT
    QML_ELEMENT

    muse::GlobalInject<au::toast::IToastService> toastService;

public:
    explicit CloudEffectModel(QObject* parent = nullptr);

    Q_INVOKABLE void apply(const QString& effectId, const QVariant& params);
};
}
