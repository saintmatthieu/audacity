/*
 * Audacity: A Digital Audio Editor
 */
#pragma once

#include <map>
#include <string>
#include <variant>
#include <vector>

#include "au3-preferences/BasicSettings.h"
#include "audioplugins/iknownaudiopluginsregister.h"
#include "modularity/ioc.h"

#include <wx/string.h>
#include <wx/arrstr.h>

namespace au::effects {
//! A BasicSettings implementation backed by KnownAudioPluginsRegister.
//! Translates PluginManager's hierarchical key/value access into
//! AudioPluginInfo reads/writes on the Muse framework plugin registry.
class PluginRegistrySettings final : public audacity::BasicSettings
{
    muse::GlobalInject<muse::audioplugins::IKnownAudioPluginsRegister> registry;

public:
    PluginRegistrySettings();
    ~PluginRegistrySettings() override;

    wxString GetGroup() const override;
    wxArrayString GetChildGroups() const override;
    wxArrayString GetChildKeys() const override;

    bool HasEntry(const wxString& key) const override;
    bool HasGroup(const wxString& key) const override;
    bool Remove(const wxString& key) override;
    void Clear() override;

    bool Read(const wxString& key, bool* value) const override;
    bool Read(const wxString& key, int* value) const override;
    bool Read(const wxString& key, long* value) const override;
    bool Read(const wxString& key, long long* value) const override;
    bool Read(const wxString& key, double* value) const override;
    bool Read(const wxString& key, wxString* value) const override;

    bool Write(const wxString& key, bool value) override;
    bool Write(const wxString& key, int value) override;
    bool Write(const wxString& key, long value) override;
    bool Write(const wxString& key, long long value) override;
    bool Write(const wxString& key, double value) override;
    bool Write(const wxString& key, const wxString& value) override;

    bool Flush() noexcept override;

protected:
    void DoBeginGroup(const wxString& prefix) override;
    void DoEndGroup() noexcept override;

private:
    using Val = std::variant<std::monostate, bool, int, long, long long, double, wxString>;

    void LoadFromRegistry();
    bool SaveToRegistry();

    std::string fullKey(const wxString& key) const;

    template<typename T>
    bool ReadValue(const wxString& key, T* value) const;

    template<typename T>
    bool WriteValue(const wxString& key, T value);

    // Base64 encoding helpers matching PluginManager::ConvertID
    static wxString EncodeID(const wxString& rawId);
    static wxString DecodeID(const wxString& base64Id);

    // Group stack for nested BeginGroup/EndGroup
    std::vector<wxString> m_groupStack;

    // Flat key-value store mirroring the hierarchical config
    std::map<std::string, Val> m_vals;
};
} // namespace au::effects
