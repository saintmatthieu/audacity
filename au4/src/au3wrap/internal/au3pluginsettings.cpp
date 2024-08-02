/*
 * Audacity: A Digital Audio Editor
 */
#include "au3Pluginsettings.h"

#include "../internal/wxtypes_convert.h"
#include <algorithm>

using namespace au::au3;

namespace {
auto ToQString(const wxString& s)
{
    return QString::fromStdString(wxToStdSting(s));
}

template<typename T>
bool ReadValue(
    const QSettings& settings, const wxString& key, T* value,
    std::function<T(const QVariant&)> converter)
{
    const QVariant val = settings.value(ToQString(key));
    if (!val.isValid()) {
        return false;
    }
    *value = converter(val);
    return true;
}

auto WoSlashes(const wxString& s)
{
    std::string group = s.ToStdString();
    if (group.front() == '/') {
        group.erase(0, 1);
    }
    if (group.back() == '/') {
        group.pop_back();
    }
    return QString::fromStdString(group);
}
} // namespace

Au3PluginSettings::Au3PluginSettings(const std::string& filename)
    : m_QSettings{QString::fromStdString(filename),
                  QSettings::defaultFormat()}
{
}

wxString Au3PluginSettings::GetGroup() const
{
    return m_QSettings.group().toStdString();
}

wxArrayString Au3PluginSettings::GetChildGroups() const
{
    const QStringList list = m_QSettings.childGroups();
    wxArrayString groups;
    std::transform(
        list.begin(), list.end(), std::back_inserter(groups),
        [](const QString& s) { return s.toStdString(); });
    return groups;
}

wxArrayString Au3PluginSettings::GetChildKeys() const
{
    const QStringList list = m_QSettings.childKeys();
    wxArrayString keys;
    std::transform(
        list.begin(), list.end(), std::back_inserter(keys),
        [](const QString& s) { return s.toStdString(); });
    return keys;
}

bool Au3PluginSettings::HasEntry(const wxString& key) const
{
    return m_QSettings.contains(ToQString(key));
}

bool Au3PluginSettings::HasGroup(const wxString& group) const
{
    return m_QSettings.childGroups().contains(WoSlashes(group));
}

bool Au3PluginSettings::Remove(const wxString& key)
{
    m_QSettings.remove(ToQString(key));
    return true;
}

void Au3PluginSettings::Clear()
{
    m_QSettings.clear();
}

bool Au3PluginSettings::Read(const wxString& key, bool* value) const
{
    return ReadValue<bool>(
        m_QSettings, key, value, [](const QVariant& v) { return v.toBool(); });
}

bool Au3PluginSettings::Read(const wxString& key, int* value) const
{
    return ReadValue<int>(
        m_QSettings, key, value, [](const QVariant& v) { return v.toInt(); });
}

bool Au3PluginSettings::Read(const wxString& key, long* value) const
{
    return ReadValue<long>(m_QSettings, key, value, [](const QVariant& v) {
        return v.toLongLong();
    });
}

bool Au3PluginSettings::Read(const wxString& key, long long* value) const
{
    return ReadValue<long long>(m_QSettings, key, value, [](const QVariant& v) {
        return v.toLongLong();
    });
}

bool Au3PluginSettings::Read(const wxString& key, double* value) const
{
    return ReadValue<double>(
        m_QSettings, key, value, [](const QVariant& v) { return v.toDouble(); });
}

bool Au3PluginSettings::Read(const wxString& key, wxString* value) const
{
    return ReadValue<wxString>(m_QSettings, key, value, [](const QVariant& v) {
        return v.toString().toStdString();
    });
}

bool Au3PluginSettings::Write(const wxString& key, bool value)
{
    m_QSettings.setValue(ToQString(key), value);
    return true;
}

bool Au3PluginSettings::Write(const wxString& key, int value)
{
    m_QSettings.setValue(ToQString(key), value);
    return true;
}

bool Au3PluginSettings::Write(const wxString& key, long value)
{
    m_QSettings.setValue(ToQString(key), QVariant::fromValue(value));
    return true;
}

bool Au3PluginSettings::Write(const wxString& key, long long value)
{
    m_QSettings.setValue(ToQString(key), value);
    return true;
}

bool Au3PluginSettings::Write(const wxString& key, double value)
{
    m_QSettings.setValue(ToQString(key), value);
    return true;
}

bool Au3PluginSettings::Write(const wxString& key, const wxString& value)
{
    m_QSettings.setValue(ToQString(key), ToQString(value));
    return true;
}

bool Au3PluginSettings::Flush() noexcept
{
    m_QSettings.sync();
    return true;
}

void Au3PluginSettings::DoBeginGroup(const wxString& prefix)
{
    m_QSettings.beginGroup(WoSlashes(prefix));
}

void Au3PluginSettings::DoEndGroup() noexcept
{
    m_QSettings.endGroup();
}
