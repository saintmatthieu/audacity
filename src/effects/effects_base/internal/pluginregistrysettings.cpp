/*
 * Audacity: A Digital Audio Editor
 */
#include "pluginregistrysettings.h"
#include "effectstypes.h"

#include <wx/base64.h>

#include "framework/global/log.h"

using namespace au::effects;

// Must match REGVERCUR in PluginManager.h
static const wxString REGVERCUR_VALUE = wxT("1.5");

// Must match REGVERKEY in PluginManager.cpp
static const std::string REGVERKEY = "/pluginregistryversion";

// Must match REGROOT in PluginManager.cpp
static const std::string REGROOT = "/pluginregistry/";

PluginRegistrySettings::PluginRegistrySettings()
{
    m_groupStack.push_back(wxT("/"));
    LoadFromRegistry();
}

PluginRegistrySettings::~PluginRegistrySettings()
{
    SaveToRegistry();
}

// -------------------------------------------------------------------
// Base64 helpers — must match PluginManager::ConvertID
// -------------------------------------------------------------------

wxString PluginRegistrySettings::EncodeID(const wxString& rawId)
{
    const wxCharBuffer& buf = rawId.ToUTF8();
    return wxT("base64:") + wxBase64Encode(buf, strlen(buf));
}

wxString PluginRegistrySettings::DecodeID(const wxString& base64Id)
{
    if (!base64Id.StartsWith(wxT("base64:"))) {
        return base64Id;
    }
    wxString data = base64Id.Mid(7);
    wxMemoryBuffer buf = wxBase64Decode(data);
    return wxString::FromUTF8(static_cast<const char*>(buf.GetData()), buf.GetDataLen());
}

// -------------------------------------------------------------------
// Load from KnownAudioPluginsRegister into flat key-value store
// -------------------------------------------------------------------

void PluginRegistrySettings::LoadFromRegistry()
{
    m_vals.clear();

    // Write the registry version
    m_vals[REGVERKEY] = REGVERCUR_VALUE;

    auto plugins = registry()->pluginInfoList();

    for (const auto& info : plugins) {
        wxString pluginId = wxString::FromUTF8(info.meta.id);
        if (pluginId.empty()) {
            continue;
        }

        wxString base64Id = EncodeID(pluginId);
        std::string groupPrefix = REGROOT + "Effect/" + base64Id.ToStdString() + "/";

        // Core fields
        m_vals[groupPrefix + "Path"] = wxString(info.path.toStdString());
        m_vals[groupPrefix + "Enabled"] = info.enabled;
        m_vals[groupPrefix + "Valid"] = (info.errorCode == 0);
        m_vals[groupPrefix + "Vendor"] = wxString::FromUTF8(info.meta.vendor);

        // Helper to read an attribute
        auto attr = [&](const muse::String& key) -> wxString {
            auto it = info.meta.attributes.find(key);
            if (it != info.meta.attributes.end()) {
                return wxString::FromUTF8(it->second.toStdString());
            }
            return wxString();
        };

        // Mapped from attributes
        m_vals[groupPrefix + "Symbol"] = attr(SYMBOL_ATTRIBUTE);
        m_vals[groupPrefix + "Name"] = attr(PLUGIN_NAME_ATTRIBUTE);
        m_vals[groupPrefix + "Version"] = attr(VERSION_ATTRIBUTE);
        m_vals[groupPrefix + "Description"] = attr(DESCRIPTION_ATTRIBUTE);
        m_vals[groupPrefix + "ProviderID"] = attr(PROVIDER_ID_ATTRIBUTE);
        m_vals[groupPrefix + "EffectType"] = attr(EFFECT_TYPE_ATTRIBUTE);
        m_vals[groupPrefix + "EffectFamily"] = attr(EFFECT_FAMILY_ATTRIBUTE);
        m_vals[groupPrefix + "EffectRealtime"] = attr(EFFECT_REALTIME_ATTRIBUTE);

        // Bool attributes stored as "1"/"0"
        auto boolAttr = [&](const muse::String& key) -> bool {
            return attr(key) == wxT("1");
        };

        m_vals[groupPrefix + "EffectDefault"] = boolAttr(EFFECT_DEFAULT_ATTRIBUTE);
        m_vals[groupPrefix + "EffectInteractive"] = boolAttr(EFFECT_INTERACTIVE_ATTRIBUTE);
        m_vals[groupPrefix + "EffectAutomatable"] = boolAttr(EFFECT_AUTOMATABLE_ATTRIBUTE);
    }
}

// -------------------------------------------------------------------
// Save from flat key-value store back to KnownAudioPluginsRegister
// -------------------------------------------------------------------

bool PluginRegistrySettings::SaveToRegistry()
{
    // Collect unique plugin group paths: /pluginregistry/Effect/base64:xxx
    std::set<std::string> pluginGroups;
    for (const auto& [key, val] : m_vals) {
        if (key.rfind(REGROOT, 0) != 0) {
            continue;
        }
        // Find third '/' after REGROOT (which ends with '/')
        // REGROOT = "/pluginregistry/" — find the type separator
        auto typeEnd = key.find('/', REGROOT.size());
        if (typeEnd == std::string::npos) {
            continue;
        }
        // Find the ID separator
        auto idEnd = key.find('/', typeEnd + 1);
        if (idEnd == std::string::npos) {
            continue;
        }
        pluginGroups.insert(key.substr(0, idEnd));
    }

    muse::audioplugins::AudioPluginInfoList infos;
    muse::audio::AudioResourceIdList ids;

    for (const auto& groupPath : pluginGroups) {
        std::string prefix = groupPath + "/";

        // Helper to read a string value from the flat store
        auto readStr = [&](const std::string& keyName) -> wxString {
            auto it = m_vals.find(prefix + keyName);
            if (it == m_vals.end()) {
                return wxString();
            }
            if (auto* s = std::get_if<wxString>(&it->second)) {
                return *s;
            }
            return wxString();
        };

        auto readBool = [&](const std::string& keyName, bool defaultVal = false) -> bool {
            auto it = m_vals.find(prefix + keyName);
            if (it == m_vals.end()) {
                return defaultVal;
            }
            if (auto* b = std::get_if<bool>(&it->second)) {
                return *b;
            }
            return defaultVal;
        };

        // Extract base64 ID from groupPath
        // groupPath = "/pluginregistry/Effect/base64:xxx"
        auto lastSlash = groupPath.rfind('/');
        if (lastSlash == std::string::npos) {
            continue;
        }
        wxString base64Id = wxString::FromUTF8(groupPath.substr(lastSlash + 1));
        wxString rawId = DecodeID(base64Id);
        if (rawId.empty()) {
            continue;
        }

        muse::audioplugins::AudioPluginInfo info;
        info.meta.id = rawId.ToStdString();
        info.path = muse::io::path_t(readStr("Path").ToStdString());
        info.enabled = readBool("Enabled");
        info.errorCode = readBool("Valid") ? 0 : 1;
        info.meta.vendor = readStr("Vendor").ToStdString();

        // Determine AudioResourceType from EffectFamily
        wxString family = readStr("EffectFamily");
        if (family == wxT("Nyquist")) {
            // info.meta.type = muse::audio::AudioResourceType::NyquistPlugin;
        } else if (family == wxT("VST") || family == wxT("VST3")) {
            info.meta.type = muse::audio::AudioResourceType::VstPlugin;
        } else if (family == wxT("LV2")) {
            info.meta.type = muse::audio::AudioResourceType::Lv2Plugin;
        } else if (family == wxT("AudioUnit")) {
            info.meta.type = muse::audio::AudioResourceType::AudioUnit;
        } else {
            // info.meta.type = muse::audio::AudioResourceType::BuiltinEffect;
        }

        // Determine AudioPluginType from EffectType
        wxString effectType = readStr("EffectType");
        if (effectType == wxT("None") || effectType.empty()) {
            info.type = muse::audioplugins::AudioPluginType::Undefined;
        } else {
            info.type = muse::audioplugins::AudioPluginType::Fx;
        }

        // Determine category attribute
        if (effectType == wxT("Process") || effectType == wxT("Generate")
            || effectType == wxT("Analyze") || effectType == wxT("Tool")) {
            info.meta.attributes.emplace(muse::audio::CATEGORIES_ATTRIBUTE, muse::String(u"Fx"));
        } else {
            info.meta.attributes.emplace(muse::audio::CATEGORIES_ATTRIBUTE, muse::String(u"None"));
        }

        // Store all effect attributes
        auto setAttr = [&](const muse::String& attrKey, const wxString& val) {
            if (!val.empty()) {
                info.meta.attributes.emplace(attrKey, muse::String::fromStdString(val.ToStdString()));
            }
        };

        setAttr(SYMBOL_ATTRIBUTE, readStr("Symbol"));
        setAttr(PLUGIN_NAME_ATTRIBUTE, readStr("Name"));
        setAttr(VERSION_ATTRIBUTE, readStr("Version"));
        setAttr(DESCRIPTION_ATTRIBUTE, readStr("Description"));
        setAttr(PROVIDER_ID_ATTRIBUTE, readStr("ProviderID"));
        setAttr(EFFECT_TYPE_ATTRIBUTE, effectType);
        setAttr(EFFECT_FAMILY_ATTRIBUTE, family);
        setAttr(EFFECT_REALTIME_ATTRIBUTE, readStr("EffectRealtime"));

        // Bool attributes
        auto setBoolAttr = [&](const muse::String& attrKey, bool val) {
            info.meta.attributes.emplace(attrKey, muse::String(val ? u"1" : u"0"));
        };

        setBoolAttr(EFFECT_DEFAULT_ATTRIBUTE, readBool("EffectDefault"));
        setBoolAttr(EFFECT_INTERACTIVE_ATTRIBUTE, readBool("EffectInteractive"));
        setBoolAttr(EFFECT_AUTOMATABLE_ATTRIBUTE, readBool("EffectAutomatable"));

        infos.push_back(std::move(info));
        ids.push_back(info.meta.id);
    }

    if (infos.empty()) {
        return true;
    }

    // At least for now, de-registering and re-registering. This is highly inefficient due
    // to the file-system r/w.
    // TODO: Extend `IKnownAudioPluginsRegister` to allow editing existing entries ?
    registry()->unregisterPlugins(ids);
    muse::Ret ret = registry()->registerPlugins(infos);
    if (!ret) {
        LOGE() << "Failed to save plugins to registry: " << ret.toString();
    }
    return ret;
}

// -------------------------------------------------------------------
// Group navigation
// -------------------------------------------------------------------

void PluginRegistrySettings::DoBeginGroup(const wxString& prefix)
{
    wxString newGroup;
    if (prefix.StartsWith(wxT("/"))) {
        newGroup = prefix;
    } else {
        if (m_groupStack.size() > 1) {
            newGroup = m_groupStack.back() + wxT("/") + prefix;
        } else {
            newGroup = wxT("/") + prefix;
        }
    }

    // Remove trailing slash
    if (!newGroup.empty() && newGroup.Last() == wxT('/')) {
        newGroup.RemoveLast();
    }

    m_groupStack.push_back(newGroup);
}

void PluginRegistrySettings::DoEndGroup() noexcept
{
    if (m_groupStack.size() > 1) {
        m_groupStack.pop_back();
    }
}

// -------------------------------------------------------------------
// Key resolution
// -------------------------------------------------------------------

std::string PluginRegistrySettings::fullKey(const wxString& key) const
{
    std::string path = key.ToStdString();
    if (m_groupStack.size() > 1) {
        return m_groupStack.back().ToStdString() + "/" + path;
    }
    // Absolute key or root-level
    if (!path.empty() && path[0] == '/') {
        return path;
    }
    return "/" + path;
}

// -------------------------------------------------------------------
// Queries
// -------------------------------------------------------------------

wxString PluginRegistrySettings::GetGroup() const
{
    if (m_groupStack.size() > 1) {
        const auto& path = m_groupStack.back();
        // Return without leading '/'
        return path.Right(path.Length() - 1);
    }
    return {};
}

wxArrayString PluginRegistrySettings::GetChildGroups() const
{
    wxArrayString result;
    std::string current;
    if (m_groupStack.size() > 1) {
        current = m_groupStack.back().ToStdString();
    }

    std::set<std::string> seen;
    for (const auto& [key, val] : m_vals) {
        std::string prefix = current.empty() ? "/" : current + "/";
        if (key.rfind(prefix, 0) != 0) {
            continue;
        }

        std::string remainder = key.substr(prefix.size());
        auto sep = remainder.find('/');
        if (sep == std::string::npos) {
            continue; // This is a key, not a subgroup
        }

        std::string subgroup = remainder.substr(0, sep);
        if (seen.insert(subgroup).second) {
            result.push_back(wxString::FromUTF8(subgroup));
        }
    }

    return result;
}

wxArrayString PluginRegistrySettings::GetChildKeys() const
{
    wxArrayString result;
    std::string current;
    if (m_groupStack.size() > 1) {
        current = m_groupStack.back().ToStdString();
    }

    for (const auto& [key, val] : m_vals) {
        std::string prefix = current.empty() ? "/" : current + "/";
        if (key.rfind(prefix, 0) != 0) {
            continue;
        }

        std::string remainder = key.substr(prefix.size());
        if (remainder.find('/') != std::string::npos) {
            continue; // This is in a subgroup, not a direct child key
        }

        result.push_back(wxString::FromUTF8(remainder));
    }

    return result;
}

bool PluginRegistrySettings::HasEntry(const wxString& key) const
{
    return m_vals.find(fullKey(key)) != m_vals.end();
}

bool PluginRegistrySettings::HasGroup(const wxString& key) const
{
    std::string prefix = fullKey(key);
    if (!prefix.empty() && prefix.back() != '/') {
        prefix += '/';
    }
    for (const auto& [k, v] : m_vals) {
        if (k.rfind(prefix, 0) == 0) {
            return true;
        }
    }
    return false;
}

bool PluginRegistrySettings::Remove(const wxString& key)
{
    std::string full = fullKey(key);
    // Remove exact key
    auto it = m_vals.find(full);
    if (it != m_vals.end()) {
        m_vals.erase(it);
        return true;
    }
    // Remove group (all keys with this prefix)
    std::string prefix = full + "/";
    bool removed = false;
    for (auto iter = m_vals.begin(); iter != m_vals.end();) {
        if (iter->first.rfind(prefix, 0) == 0) {
            iter = m_vals.erase(iter);
            removed = true;
        } else {
            ++iter;
        }
    }
    return removed;
}

void PluginRegistrySettings::Clear()
{
    m_vals.clear();
}

// -------------------------------------------------------------------
// Read
// -------------------------------------------------------------------

template<typename T>
bool PluginRegistrySettings::ReadValue(const wxString& key, T* value) const
{
    std::string full = fullKey(key);
    auto it = m_vals.find(full);
    if (it == m_vals.end()) {
        return false;
    }
    if (auto* v = std::get_if<T>(&it->second)) {
        *value = *v;
        return true;
    }
    return false;
}

bool PluginRegistrySettings::Read(const wxString& key, bool* value) const
{
    return ReadValue(key, value);
}

bool PluginRegistrySettings::Read(const wxString& key, int* value) const
{
    return ReadValue(key, value);
}

bool PluginRegistrySettings::Read(const wxString& key, long* value) const
{
    return ReadValue(key, value);
}

bool PluginRegistrySettings::Read(const wxString& key, long long* value) const
{
    return ReadValue(key, value);
}

bool PluginRegistrySettings::Read(const wxString& key, double* value) const
{
    return ReadValue(key, value);
}

bool PluginRegistrySettings::Read(const wxString& key, wxString* value) const
{
    return ReadValue(key, value);
}

// -------------------------------------------------------------------
// Write
// -------------------------------------------------------------------

template<typename T>
bool PluginRegistrySettings::WriteValue(const wxString& key, T value)
{
    std::string full = fullKey(key);
    m_vals[full] = value;
    return true;
}

bool PluginRegistrySettings::Write(const wxString& key, bool value)
{
    return WriteValue(key, value);
}

bool PluginRegistrySettings::Write(const wxString& key, int value)
{
    return WriteValue(key, value);
}

bool PluginRegistrySettings::Write(const wxString& key, long value)
{
    return WriteValue(key, value);
}

bool PluginRegistrySettings::Write(const wxString& key, long long value)
{
    return WriteValue(key, value);
}

bool PluginRegistrySettings::Write(const wxString& key, double value)
{
    return WriteValue(key, value);
}

bool PluginRegistrySettings::Write(const wxString& key, const wxString& value)
{
    return WriteValue(key, value);
}

// -------------------------------------------------------------------
// Flush
// -------------------------------------------------------------------

bool PluginRegistrySettings::Flush() noexcept
{
    return SaveToRegistry();
}
