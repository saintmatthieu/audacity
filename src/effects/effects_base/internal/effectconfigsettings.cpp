/*
 * Audacity: A Digital Audio Editor
 */
#include "effectconfigsettings.h"

#include "global/serialization/json.h"
#include "global/io/file.h"

#include "au3wrap/internal/wxtypes_convert.h"
#include "log.h"

#include <cassert>
#include <set>

using namespace muse;
using namespace au::au3;

EffectConfigSettings::EffectConfigSettings(const std::string& filename)
    : m_filename(filename)
{
    m_groupStack.push_back("/");
    Load();
}

EffectConfigSettings::~EffectConfigSettings()
{
    Save();
}

void EffectConfigSettings::Load()
{
    ByteArray json;
    Ret ret = io::File::readFile(m_filename, json);
    if (!ret) {
        LOGE() << "failed read file: " << m_filename << ", err: " << ret.toString();
        return;
    }

    m_vals.clear();

    std::string err;
    JsonArray arr = JsonDocument::fromJson(json, &err).rootArray();

    if (!err.empty()) {
        LOGE() << "failed parse json from file: " << m_filename << ", err: " << err;
        return;
    }

    for (size_t i = 0; i < arr.size(); ++i) {
        JsonObject obj = arr.at(i).toObject();
        if (obj.empty()) {
            continue;
        }

        std::string key = obj.value("key").toStdString();
        std::string type = obj.value("type").toStdString();
        JsonValue val = obj.value("val");
        if (type == "bool") {
            m_vals.insert({ key, val.toBool() });
        } else if (type == "int") {
            m_vals.insert({ key, val.toInt() });
        } else if (type == "long") {
            m_vals.insert({ key, std::stol(val.toStdString()) });
        } else if (type == "long long") {
            m_vals.insert({ key, std::stoll(val.toStdString()) });
        } else if (type == "double") {
            m_vals.insert({ key, val.toDouble() });
        } else if (type == "string") {
            m_vals.insert({ key, val.toStdString() });
        }
    }
}

bool EffectConfigSettings::Save()
{
    JsonArray arr;
    for (const auto& p : m_vals) {
        JsonObject obj;
        const std::string& key = p.first;
        std::visit([&key, &obj](auto&& v) {
            using T = std::decay_t<decltype(v)>;
            // std::monostate, bool, int, long, long long, double, wxString
            if constexpr (std::is_same_v<T, std::monostate>) {
                return;
            } else if constexpr (std::is_same_v<T, bool>) {
                obj["key"] = key;
                obj["val"] = v;
                obj["type"] = "bool";
            } else if constexpr (std::is_same_v<T, int>) {
                obj["key"] = key;
                obj["val"] = v;
                obj["type"] = "int";
            } else if constexpr (std::is_same_v<T, long>) {
                obj["key"] = key;
                obj["val"] = std::to_string(v);
                obj["type"] = "long";
            } else if constexpr (std::is_same_v<T, long long>) {
                obj["key"] = key;
                obj["val"] = std::to_string(v);
                obj["type"] = "long long";
            } else if constexpr (std::is_same_v<T, double>) {
                obj["key"] = key;
                obj["val"] = v;
                obj["type"] = "double";
            } else if constexpr (std::is_same_v<T, wxString>) {
                obj["key"] = key;
                obj["val"] = au::au3::wxToStdString(v);
                obj["type"] = "string";
            }
        }, p.second);

        arr.append(obj);
    }

    ByteArray json = JsonDocument(arr).toJson();

    Ret ret = io::File::writeFile(m_filename, json);
    if (!ret) {
        LOGE() << "failed write to file: " << m_filename << ", err: " << ret.toString();
    }

    return ret;
}

std::string EffectConfigSettings::fullKey(const wxString& key) const
{
    if (key.starts_with('/')) {
        return key.ToStdString();
    }
    if (m_groupStack.size() > 1) {
        return m_groupStack.back() + "/" + au3::wxToStdString(key);
    }
    return "/" + au3::wxToStdString(key);
}

wxString EffectConfigSettings::GetGroup() const
{
    assert(!m_groupStack.empty());
    if (m_groupStack.size() > 1) {
        const auto path = wxString{ m_groupStack.back() };
        return path.Right(path.Length() - 1);
    }
    return {};
}

wxArrayString EffectConfigSettings::GetChildGroups() const
{
    wxArrayString children;
    if (m_groupStack.empty()) {
        return children;
    }

    const std::string& currentPath = m_groupStack.back();

    std::set<std::string> seen;
    for (const auto& p : m_vals) {
        const std::string& key = p.first;
        if (key.compare(0, currentPath.size(), currentPath) != 0) {
            continue;
        }

        std::string remainder = key.substr(currentPath.size());
        size_t sep = remainder.find('/');
        if (sep == std::string::npos) {
            continue;
        }

        std::string groupName = remainder.substr(0, sep);
        if (seen.insert(groupName).second) {
            children.push_back(groupName);
        }
    }

    return children;
}

wxArrayString EffectConfigSettings::GetChildKeys() const
{
    wxArrayString child;
    const std::string& currentPath = m_groupStack.back();
    const std::string prefix = (currentPath == "/") ? "/" : currentPath + "/";

    for (const auto& p : m_vals) {
        const std::string& key = p.first;
        if (key.compare(0, prefix.size(), prefix) != 0) {
            continue;
        }

        std::string remainder = key.substr(prefix.size());
        if (remainder.find('/') != std::string::npos) {
            continue;
        }

        child.push_back(remainder);
    }

    return child;
}

bool EffectConfigSettings::HasEntry(const wxString& key) const
{
    std::string full = fullKey(key);
    return m_vals.find(full) != m_vals.end();
}

bool EffectConfigSettings::HasGroup(const wxString& group) const
{
    const std::string full = fullKey(group);
    for (const auto& p : m_vals) {
        if (p.first.compare(0, full.size(), full) == 0) {
            return true;
        }
    }
    return false;
}

bool EffectConfigSettings::Remove(const wxString& key)
{
    if (key.empty()) {
        const std::string& currentPath = m_groupStack.back();
        const std::string prefix = (currentPath == "/") ? "/" : currentPath + "/";
        std::vector<std::string> toRemoveKeys;
        for (const auto& p : m_vals) {
            if (p.first.compare(0, prefix.size(), prefix) == 0) {
                toRemoveKeys.push_back(p.first);
            }
        }
        for (const std::string& k : toRemoveKeys) {
            m_vals.erase(k);
        }
        return true;
    }

    const std::string full = fullKey(key);

    auto it = m_vals.find(full);
    if (it != m_vals.end()) {
        m_vals.erase(it);
        return true;
    }

    const std::string prefix = full + "/";
    std::vector<std::string> toRemoveKeys;
    for (const auto& p : m_vals) {
        if (p.first.compare(0, prefix.size(), prefix) == 0) {
            toRemoveKeys.push_back(p.first);
        }
    }
    if (toRemoveKeys.empty()) {
        return false;
    }
    for (const std::string& k : toRemoveKeys) {
        m_vals.erase(k);
    }
    return true;
}

void EffectConfigSettings::Clear()
{
    m_vals.clear();
}

bool EffectConfigSettings::Read(const wxString& key, bool* value) const
{
    return ReadValue(key, value);
}

bool EffectConfigSettings::Read(const wxString& key, int* value) const
{
    return ReadValue(key, value);
}

bool EffectConfigSettings::Read(const wxString& key, long* value) const
{
    return ReadValue(key, value);
}

bool EffectConfigSettings::Read(const wxString& key, long long* value) const
{
    return ReadValue(key, value);
}

bool EffectConfigSettings::Read(const wxString& key, double* value) const
{
    return ReadValue(key, value);
}

bool EffectConfigSettings::Read(const wxString& key, wxString* value) const
{
    return ReadValue(key, value);
}

bool EffectConfigSettings::Write(const wxString& key, bool value)
{
    return WriteValue(key, value);
}

bool EffectConfigSettings::Write(const wxString& key, int value)
{
    return WriteValue(key, value);
}

bool EffectConfigSettings::Write(const wxString& key, long value)
{
    return WriteValue(key, value);
}

bool EffectConfigSettings::Write(const wxString& key, long long value)
{
    return WriteValue(key, value);
}

bool EffectConfigSettings::Write(const wxString& key, double value)
{
    return WriteValue(key, value);
}

bool EffectConfigSettings::Write(const wxString& key, const wxString& value)
{
    return WriteValue(key, value);
}

bool EffectConfigSettings::Flush() noexcept
{
    return Save();
}

void EffectConfigSettings::DoBeginGroup(const wxString& wxPrefix)
{
    const auto prefix = wxPrefix.ToStdString();
    if (wxPrefix.starts_with('/')) {
        m_groupStack.push_back(prefix);
    } else {
        if (m_groupStack.size() > 1) {
            m_groupStack.push_back(m_groupStack.back() + prefix);
        } else {
            m_groupStack.push_back(std::string { "/" } + prefix);
        }
    }
}

void EffectConfigSettings::DoEndGroup() noexcept
{
    assert(m_groupStack.size() > 1);// "No matching DoBeginGroup"

    if (m_groupStack.size() > 1) {
        m_groupStack.pop_back();
    }
}
