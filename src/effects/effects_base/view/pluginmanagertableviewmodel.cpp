/*
 * Audacity: A Digital Audio Editor
 */

 #include "pluginmanagertableviewmodel.h"

#include "framework/global/translation.h"
#include "framework/uicomponents/qml/Muse/UiComponents/internal/tableviewcell.h"

namespace au::effects {
namespace {
QString effectFamilyToString(EffectFamily family)
{
    switch (family) {
    case EffectFamily::Builtin: return QStringLiteral("Audacity");
    case EffectFamily::VST3: return QStringLiteral("VST3");
    case EffectFamily::LV2: return QStringLiteral("LV2");
    case EffectFamily::AudioUnit: return QStringLiteral("AudioUnit");
    case EffectFamily::Nyquist: return QStringLiteral("Nyquist");
    default: return QStringLiteral("Unknown");
    }
}
}

PluginManagerTableViewModel::PluginManagerTableViewModel(QObject* parent)
    : AbstractTableViewModel(parent), muse::Injectable(muse::iocCtxForQmlObject(this)) {}

void PluginManagerTableViewModel::componentComplete()
{
    effectsProvider()->effectMetaListChanged().onNotify(this, [this]() {
        reload();
    });
    reload();
}

void PluginManagerTableViewModel::reload()
{
    setHorizontalHeaders(makeHorizontalHeaders());
    setVerticalHeaders(makeVerticalHeaders());
    setTable(makeTable());
}

QVector<muse::uicomponents::TableViewHeader*> PluginManagerTableViewModel::makeHorizontalHeaders()
{
    using namespace muse::uicomponents;

    QVector<TableViewHeader*> hHeaders;

    hHeaders << makeHorizontalHeader(muse::qtrc("effects", "Enabled"),
                                     static_cast<TableViewCellType::Type>(PluginManagerTableViewCellType::Type::Enabled),
                                     TableViewCellEditMode::Mode::StartInEdit, 100);
    hHeaders << makeHorizontalHeader(muse::qtrc("effects", "Name"),
                                     TableViewCellType::Type::String, TableViewCellEditMode::Mode::DoubleClick, 152);
    hHeaders << makeHorizontalHeader(muse::qtrc("effects", "Path"),
                                     TableViewCellType::Type::String, TableViewCellEditMode::Mode::DoubleClick, 296);
    hHeaders << makeHorizontalHeader(muse::qtrc("effects", "Type"),
                                     TableViewCellType::Type::String, TableViewCellEditMode::Mode::DoubleClick, 152);

    return hHeaders;
}

QVector<muse::uicomponents::TableViewHeader*> PluginManagerTableViewModel::makeVerticalHeaders()
{
    using namespace muse::uicomponents;

    const EffectMetaList effects = effectsProvider()->effectMetaList();
    QVector<TableViewHeader*> vHeaders;

    for (const auto& _ : effects) {
        vHeaders << new TableViewHeader(this);
    }

    return vHeaders;
}

QVector<QVector<muse::uicomponents::TableViewCell*> > PluginManagerTableViewModel::makeTable()
{
    const EffectMetaList effects = effectsProvider()->effectMetaList();
    QVector<QVector<muse::uicomponents::TableViewCell*> > table;

    for (const auto& meta : effects) {
        QVector<muse::uicomponents::TableViewCell*> row;
        row.append(makeCell(muse::Val(meta.isEnabled)));
        row.append(makeCell(muse::Val(meta.title.toQString())));
        row.append(makeCell(muse::Val(meta.path.toQString())));
        row.append(makeCell(muse::Val(effectFamilyToString(meta.family))));
        table.append(row);
    }

    return table;
}

bool PluginManagerTableViewModel::doCellValueChangeRequested(int row, int column, const muse::Val& value)
{
    if (column != 0 || row < 0) {
        return false;
    }

    const EffectMetaList effects = effectsProvider()->effectMetaList();
    if (row >= static_cast<int>(effects.size())) {
        return false;
    }

    effectsProvider()->setEffectEnabled(effects[row].id, value.toBool());
    return true;
}
}
