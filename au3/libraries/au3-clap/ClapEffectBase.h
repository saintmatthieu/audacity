/**********************************************************************

  Audacity: A Digital Audio Editor

  @file ClapEffectBase.h

  @brief Connects Audacity's effect framework with a CLAP plugin descriptor.

**********************************************************************/
#pragma once

#include <memory>
#include <string>

#include "au3-effects/PerTrackEffect.h"

struct clap_plugin_descriptor;
class ClapEntry;

class CLAP_API ClapEffectBase : public PerTrackEffect
{
public:
    static EffectFamilySymbol GetFamilySymbol();

    ClapEffectBase(std::shared_ptr<ClapEntry> entry, const clap_plugin_descriptor& descriptor);
    ~ClapEffectBase() override;

    ClapEffectBase(const ClapEffectBase&) = delete;
    ClapEffectBase& operator=(const ClapEffectBase&) = delete;

    // ComponentInterface
    PluginPath GetPath() const override;
    ComponentInterfaceSymbol GetSymbol() const override;
    VendorSymbol GetVendor() const override;
    wxString GetVersion() const override;
    TranslatableString GetDescription() const override;

    // EffectDefinitionInterface
    EffectType GetType() const override;
    EffectFamilySymbol GetFamily() const override;
    bool IsInteractive() const override;
    bool IsDefault() const override;
    RealtimeSince RealtimeSupport() const override;
    bool SupportsAutomation() const override;
    bool SaveSettings(const EffectSettings& settings, CommandParameters& parms) const override;
    bool LoadSettings(const CommandParameters& parms, EffectSettings& settings) const override;
    OptionalMessage LoadUserPreset(const RegistryPath& name, EffectSettings& settings) const override;
    bool SaveUserPreset(const RegistryPath& name, const EffectSettings& settings) const override;
    RegistryPaths GetFactoryPresets() const override;
    OptionalMessage LoadFactoryPreset(int id, EffectSettings& settings) const override;

    // EffectInstanceFactory
    std::shared_ptr<EffectInstance> MakeInstance() const override;
    bool CanExportPresets() const override;
    bool HasOptions() const override;

    // EffectSettingsManager
    EffectSettings MakeSettings() const override;
    bool CopySettingsContents(const EffectSettings& src, EffectSettings& dst) const override;

    //! Accessors used by the module loader / validator.
    std::shared_ptr<ClapEntry> entry() const { return mEntry; }
    const std::string& pluginId() const { return mPluginId; }

private:
    std::shared_ptr<ClapEntry> mEntry;
    std::string mPluginId;
    wxString mName;
    wxString mVendor;
    wxString mVersion;
    TranslatableString mDescription;
    EffectType mType { EffectTypeProcess };
};
