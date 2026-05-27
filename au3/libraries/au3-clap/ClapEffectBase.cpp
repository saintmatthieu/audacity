/**********************************************************************

  Audacity: A Digital Audio Editor

  @file ClapEffectBase.cpp

**********************************************************************/
#include "ClapEffectBase.h"

#include <clap/plugin.h>

#include "ClapEntry.h"
#include "ClapInstance.h"
#include "ClapUtils.h"
#include "ClapWrapper.h"

EffectFamilySymbol ClapEffectBase::GetFamilySymbol()
{
    return XO("CLAP");
}

ClapEffectBase::ClapEffectBase(std::shared_ptr<ClapEntry> entry, const clap_plugin_descriptor& descriptor)
    : mEntry(std::move(entry)), mPluginId(descriptor.id ? descriptor.id : "")
{
    mName = descriptor.name ? wxString::FromUTF8(descriptor.name) : wxString(mPluginId);
    mVendor = descriptor.vendor ? wxString::FromUTF8(descriptor.vendor) : wxString {};
    mVersion = descriptor.version ? wxString::FromUTF8(descriptor.version) : wxString {};
    if (descriptor.description && *descriptor.description) {
        mDescription = Verbatim(wxString::FromUTF8(descriptor.description));
    }
    mType = ClapUtils::EffectTypeFromFeatures(descriptor.features);
}

ClapEffectBase::~ClapEffectBase() = default;

PluginPath ClapEffectBase::GetPath() const
{
    return ClapUtils::MakePluginPathString(mEntry->path(), mPluginId);
}

ComponentInterfaceSymbol ClapEffectBase::GetSymbol() const
{
    return mName;
}

VendorSymbol ClapEffectBase::GetVendor() const
{
    return mVendor;
}

wxString ClapEffectBase::GetVersion() const
{
    return mVersion;
}

TranslatableString ClapEffectBase::GetDescription() const
{
    return mDescription;
}

EffectType ClapEffectBase::GetType() const
{
    return mType;
}

EffectFamilySymbol ClapEffectBase::GetFamily() const
{
    return GetFamilySymbol();
}

bool ClapEffectBase::IsInteractive() const
{
    return true;
}

bool ClapEffectBase::IsDefault() const
{
    return false;
}

auto ClapEffectBase::RealtimeSupport() const -> RealtimeSince
{
    return mType == EffectTypeProcess ? RealtimeSince::After_3_1 : RealtimeSince::Never;
}

bool ClapEffectBase::SupportsAutomation() const
{
    return true;
}

bool ClapEffectBase::SaveSettings(const EffectSettings& settings, CommandParameters& parms) const
{
    ClapWrapper::SaveSettings(settings, parms);
    return true;
}

bool ClapEffectBase::LoadSettings(const CommandParameters& parms, EffectSettings& settings) const
{
    ClapWrapper::LoadSettings(parms, settings);
    return true;
}

OptionalMessage ClapEffectBase::LoadUserPreset(const RegistryPath& name, EffectSettings& settings) const
{
    return ClapWrapper::LoadUserPreset(*this, name, settings);
}

bool ClapEffectBase::SaveUserPreset(const RegistryPath& name, const EffectSettings& settings) const
{
    ClapWrapper::SaveUserPreset(*this, name, settings);
    return true;
}

RegistryPaths ClapEffectBase::GetFactoryPresets() const
{
    // Factory preset enumeration (clap.preset-discovery) is deferred.
    return {};
}

OptionalMessage ClapEffectBase::LoadFactoryPreset(int, EffectSettings&) const
{
    return {};
}

std::shared_ptr<EffectInstance> ClapEffectBase::MakeInstance() const
{
    return std::make_shared<ClapInstance>(*this, mEntry, mPluginId);
}

bool ClapEffectBase::CanExportPresets() const
{
    return false;
}

bool ClapEffectBase::HasOptions() const
{
    return false;
}

EffectSettings ClapEffectBase::MakeSettings() const
{
    return ClapWrapper::MakeSettings();
}

bool ClapEffectBase::CopySettingsContents(const EffectSettings& src, EffectSettings& dst) const
{
    ClapWrapper::CopySettingsContents(src, dst);
    return true;
}
