/**********************************************************************

  Audacity: A Digital Audio Editor

  @file ClapEffectsProvider.h

  @brief PluginProvider that discovers and loads CLAP effects.

  Kept free of the CLAP SDK so the AU4 effects module can include it.

**********************************************************************/
#pragma once

#include <memory>
#include <unordered_map>

#include "au3-components/PluginProvider.h"

class ClapEntry;

namespace BasicUI {
class ProgressDialog;
}

class CLAP_API ClapEffectsProvider final : public PluginProvider
{
public:
    PluginPath GetPath() const override;
    ComponentInterfaceSymbol GetSymbol() const override;
    VendorSymbol GetVendor() const override;
    wxString GetVersion() const override;
    TranslatableString GetDescription() const override;

    bool Initialize() override;
    void Terminate() override;
    EffectFamilySymbol GetOptionalFamilySymbol() override;
    const FileExtensions& GetFileExtensions() override;
    FilePath InstallPath() override;
    void AutoRegisterPlugins(PluginManagerInterface& pluginManager) override;
    bool SupportsCustomModulePaths() const override;
    PluginPaths FindModulePaths(PluginManagerInterface& pluginManager, BasicUI::ProgressDialog* progress = nullptr) const override;
    unsigned DiscoverPluginsAtPath(const PluginPath& path, TranslatableString& errMsg, const RegistrationCallback& callback) override;
    bool CheckPluginExist(const PluginPath& path) const override;
    std::unique_ptr<ComponentInterface> LoadPlugin(const PluginPath& path) override;
    std::unique_ptr<Validator> MakeValidator() const override;

private:
    //! Look up an already-loaded entry, or load it from disk.
    std::shared_ptr<ClapEntry> GetEntry(const wxString& path);

    //! Weak cache of loaded entries keyed by module path (mirrors VST3EffectsModule).
    std::unordered_map<wxString, std::weak_ptr<ClapEntry> > mEntries;
};
