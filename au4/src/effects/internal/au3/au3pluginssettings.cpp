/*
* Audacity: A Digital Audio Editor
*/
#include "au3pluginssettings.h"

#include "au3wrap/internal/wxtypes_convert.h"

#include "global/settings.h"
#include "log.h"

using namespace au::au3;

static muse::Settings::Key make_key(const wxString& key)
{
    return muse::Settings::Key("au3wrap", wxToStdSting(key));
}

Au3PluginsSettings::Au3PluginsSettings()
{
}

wxString Au3PluginsSettings::GetGroup() const
{
    NOT_IMPLEMENTED;
    return wxString("mock_group");
}

wxArrayString Au3PluginsSettings::GetChildGroups() const
{
    NOT_IMPLEMENTED;
    return {};
}

wxArrayString Au3PluginsSettings::GetChildKeys() const
{
    NOT_IMPLEMENTED;
    return {};
}

bool Au3PluginsSettings::HasEntry(const wxString& key) const
{
    // muse::Val val = muse::settings()->value(make_key(key));
    // if (val.isNull()) {
    //     return false;
    // }
    return true;
}

bool Au3PluginsSettings::HasGroup(const wxString& key) const
{
    NOT_IMPLEMENTED << ", key: " << wxToStdSting(key);
    return true;
}

bool Au3PluginsSettings::Remove(const wxString& key)
{
    muse::settings()->setLocalValue(make_key(key), muse::Val());
    return true;
}

void Au3PluginsSettings::Clear()
{
    // muse::settings()->reset(false);
}

void Au3PluginsSettings::DoBeginGroup(const wxString& prefix)
{
    NOT_IMPLEMENTED << ", prefix: " << wxToStdSting(prefix);
}

void Au3PluginsSettings::DoEndGroup() noexcept
{
    NOT_IMPLEMENTED;
}

bool Au3PluginsSettings::Flush() noexcept
{
    return true;
}

bool Au3PluginsSettings::Read(const wxString& key, bool* value) const
{
    // muse::Val val = muse::settings()->value(make_key(key));
    // if (val.isNull()) {
    //     return false;
    // }

    // *value = val.toBool();
    return true;
}

bool Au3PluginsSettings::Read(const wxString& key, int* value) const
{
    // muse::Val val = muse::settings()->value(make_key(key));
    // if (val.isNull()) {
    //     return false;
    // }

    // *value = val.toInt();
    return true;
}

bool Au3PluginsSettings::Read(const wxString& key, long* value) const
{
    // muse::Val val = muse::settings()->value(make_key(key));
    // if (val.isNull()) {
    //     return false;
    // }

    // *value = val.toInt64();
    return true;
}

bool Au3PluginsSettings::Read(const wxString& key, long long* value) const
{
    // muse::Val val = muse::settings()->value(make_key(key));
    // if (val.isNull()) {
    //     return false;
    // }

    // *value = val.toInt64();
    return true;
}

bool Au3PluginsSettings::Read(const wxString& key, double* value) const
{
    // muse::Val val = muse::settings()->value(make_key(key));
    // if (val.isNull()) {
    //     return false;
    // }

    // *value = val.toDouble();
    return true;
}

bool Au3PluginsSettings::Read(const wxString& key, wxString* value) const
{
    // muse::Val val = muse::settings()->value(make_key(key));
    // if (val.isNull()) {
    //     return false;
    // }

    // *value = val.toString();
    return true;
}

bool Au3PluginsSettings::Write(const wxString& key, bool value)
{
    // muse::settings()->setLocalValue(make_key(key), muse::Val(value));
    return true;
}

bool Au3PluginsSettings::Write(const wxString& key, int value)
{
    // muse::settings()->setLocalValue(make_key(key), muse::Val(value));
    return true;
}

bool Au3PluginsSettings::Write(const wxString& key, long value)
{
    // muse::settings()->setLocalValue(make_key(key), muse::Val(static_cast<int64_t>(value)));
    return true;
}

bool Au3PluginsSettings::Write(const wxString& key, long long value)
{
    // muse::settings()->setLocalValue(make_key(key), muse::Val(static_cast<int64_t>(value)));
    return true;
}

bool Au3PluginsSettings::Write(const wxString& key, double value)
{
    // muse::settings()->setLocalValue(make_key(key), muse::Val(value));
    return true;
}

bool Au3PluginsSettings::Write(const wxString& key, const wxString& value)
{
    // muse::settings()->setLocalValue(make_key(key), muse::Val(wxToStdSting(value)));
    return true;
}
