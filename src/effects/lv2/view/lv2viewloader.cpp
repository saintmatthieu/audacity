/*
 * Audacity: A Digital Audio Editor
 */
#include "lv2viewloader.h"
#include "effects/effects_base/effectstypes.h"

#include <QQmlEngine>

#include "au3wrap/internal/wxtypes_convert.h"
#include "global/types/number.h"
#include "libraries/lib-effects/EffectManager.h"
#include "libraries/lib-effects/Effect.h"

#include "log.h"

namespace au::effects {
Lv2ViewLoader::Lv2ViewLoader(QObject* parent)
    : QObject(parent)
{}

Lv2ViewLoader::~Lv2ViewLoader()
{
    if (m_contentItem) {
        m_contentItem->deleteLater();
        m_contentItem = nullptr;
    }
}

void Lv2ViewLoader::load(const QString& instanceId, QObject* itemParent)
{
    LOGD() << "instanceId: " << instanceId;

    // TODO: could instancesRegister have a `typeByInstanceId` method?
    const auto effectId = instancesRegister()->effectIdByInstanceId(instanceId.toInt()).toStdString();

    const auto effect = dynamic_cast<Effect*>(EffectManager::Get().GetEffect(effectId));
    IF_ASSERT_FAILED(effect) {
        LOGE() << "effect not available, instanceId: " << instanceId << ", effectId: " << effectId;
        return;
    }

    const muse::String type = au3::wxToString(effect->GetSymbol().Internal());

    QString url = viewRegister()->viewUrl(type);
    if (url.isEmpty()) {
        LOGE() << "Not found view for type: " << type;
        return;
    }
    LOGD() << "found view for type: " << type << ", url: " << url;

    QQmlEngine* qmlEngine = engine()->qmlEngine();

    //! NOTE We create extension UI using a separate engine to control what we provide,
    //! making it easier to maintain backward compatibility and stability.
    QQmlComponent component = QQmlComponent(qmlEngine, url);
    if (!component.isReady()) {
        LOGE() << "Failed to load QML file: " << url;
        LOGE() << component.errorString();
        return;
    }

    QObject* obj = component.createWithInitialProperties(
    {
        { "parent", QVariant::fromValue(itemParent) },
        { "instanceId", QVariant::fromValue(instanceId) }
    });

    m_contentItem = qobject_cast<QQuickItem*>(obj);
    if (!m_contentItem) {
        LOGE() << "Component not QuickItem, file: " << url;
    }

    if (m_contentItem) {
        if (muse::is_zero(m_contentItem->implicitHeight())) {
            m_contentItem->setImplicitHeight(m_contentItem->height());
            if (muse::is_zero(m_contentItem->implicitHeight())) {
                m_contentItem->setImplicitHeight(450);
            }
        }

        if (muse::is_zero(m_contentItem->implicitWidth())) {
            m_contentItem->setImplicitWidth(m_contentItem->width());
            if (muse::is_zero(m_contentItem->implicitWidth())) {
                m_contentItem->setImplicitWidth(200);
            }
        }
    }

    emit contentItemChanged();
}

QQuickItem* Lv2ViewLoader::contentItem() const
{
    return m_contentItem;
}
}