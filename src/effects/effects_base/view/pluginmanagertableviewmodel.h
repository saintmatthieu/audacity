/*
 * Audacity: A Digital Audio Editor
 */
#pragma once

#include "ieffectsprovider.h"

#include "framework/global/modularity/ioc.h"
#include "framework/uicomponents/qml/Muse/UiComponents/abstracttableviewmodel.h"

#include <QQmlParserStatus>
#include <QObject>
#include <qtmetamacros.h>

namespace au::effects {
namespace PluginManagerTableViewCellType {
Q_NAMESPACE;
QML_ELEMENT;

enum class Type {
    Enabled = static_cast<int>(muse::uicomponents::TableViewCellType::Type::UserType) + 1,
};

Q_ENUM_NS(Type)
}

class PluginManagerTableViewModel : public muse::uicomponents::AbstractTableViewModel, public QQmlParserStatus, public muse::Injectable
{
    Q_OBJECT
    Q_INTERFACES(QQmlParserStatus)

    muse::Inject<IEffectsProvider> effectsProvider{ this };

public:
    explicit PluginManagerTableViewModel(QObject* parent = nullptr);

private:
    void classBegin() override {}
    void componentComplete() override;
    bool doCellValueChangeRequested(int row, int column, const muse::Val& value) override;
    void reload();

    QVector<muse::uicomponents::TableViewHeader*> makeHorizontalHeaders();
    QVector<muse::uicomponents::TableViewHeader*> makeVerticalHeaders();
    QVector<QVector<muse::uicomponents::TableViewCell*> > makeTable();
};
}
