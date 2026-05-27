/**********************************************************************

  Audacity: A Digital Audio Editor

  @file ClapEffectsProvider.cpp

**********************************************************************/
#include "ClapEffectsProvider.h"

#include <stdexcept>

#include <wx/dir.h>
#include <wx/filename.h>
#include <wx/log.h>
#include <wx/utils.h>

#include <clap/clap.h>

#include "au3-basic-ui/BasicUI.h"
#include "au3-files/PlatformCompatibility.h"
#include "au3-module-manager/ModuleManager.h"
#include "au3-module-manager/PluginInterface.h"
#include "au3-strings/wxArrayStringEx.h"

#include "ClapEffectBase.h"
#include "ClapEntry.h"
#include "ClapUtils.h"
#include "ClapWrapper.h"

DECLARE_PROVIDER_ENTRY(AudacityModule)
{
    return std::make_unique<ClapEffectsProvider>();
}

DECLARE_BUILTIN_PROVIDER(ClapBuiltin);

namespace {
#ifdef __WXMSW__
constexpr wxChar kEnvPathSeparator = wxT(';');
#else
constexpr wxChar kEnvPathSeparator = wxT(':');
#endif

//! Collects ".clap" files (and, on macOS, ".clap" bundle directories) during a
//! recursive directory walk.
class ClapPluginTraverser final : public wxDirTraverser
{
public:
    ClapPluginTraverser(std::function<void(const wxString&)> onFound, BasicUI::ProgressDialog* progress)
        : mOnFound(std::move(onFound)), mProgress(progress) {}

    bool cancelled() const { return mCancelled; }

    wxDirTraverseResult OnFile(const wxString& filename) override
    {
        if (!poll(filename)) {
            return wxDIR_STOP;
        }
        if (filename.Matches(wxT("*.clap"))) {
            mOnFound(filename);
        }
        return wxDIR_CONTINUE;
    }

    wxDirTraverseResult OnDir(const wxString& dirname) override
    {
        if (!poll(dirname)) {
            return wxDIR_STOP;
        }
        if (dirname.Matches(wxT("*.clap"))) {
            // macOS bundle: report it and do not descend into its internals.
            mOnFound(dirname);
            return wxDIR_IGNORE;
        }
        return wxDIR_CONTINUE;
    }

private:
    bool poll(const wxString& path)
    {
        if (!mProgress) {
            return true;
        }
        const auto result = mProgress->Poll(0, 0, XO("Searching CLAP in: %s").Format(path));
        if (result == BasicUI::ProgressResult::Cancelled) {
            mCancelled = true;
            return false;
        }
        return true;
    }

    std::function<void(const wxString&)> mOnFound;
    BasicUI::ProgressDialog* mProgress { nullptr };
    bool mCancelled { false };
};

void appendStandardPaths(FilePaths& pathList)
{
#ifdef __WXMSW__
    wxString commonFiles;
    if (wxGetEnv(wxT("COMMONPROGRAMFILES"), &commonFiles)) {
        pathList.push_back(commonFiles + wxT("\\CLAP"));
    }
    wxString localAppData;
    if (wxGetEnv(wxT("LOCALAPPDATA"), &localAppData)) {
        pathList.push_back(localAppData + wxT("\\Programs\\Common\\CLAP"));
    }
#elif defined(__WXMAC__)
    pathList.push_back(wxGetHomeDir() + wxT("/Library/Audio/Plug-Ins/CLAP"));
    pathList.push_back(wxT("/Library/Audio/Plug-Ins/CLAP"));
#else
    pathList.push_back(wxGetHomeDir() + wxT("/.clap"));
    pathList.push_back(wxT("/usr/lib/clap"));
    pathList.push_back(wxT("/usr/local/lib/clap"));
#endif

    // The CLAP_PATH environment variable, OS-PATH formatted.
    wxString clapPath;
    if (wxGetEnv(wxT("CLAP_PATH"), &clapPath) && !clapPath.empty()) {
        wxString rest = clapPath;
        while (!rest.empty()) {
            const wxString dir = rest.BeforeFirst(kEnvPathSeparator, &rest);
            if (!dir.empty()) {
                pathList.push_back(dir);
            }
        }
    }
}
}

std::shared_ptr<ClapEntry> ClapEffectsProvider::GetEntry(const wxString& path)
{
    const auto it = mEntries.find(path);
    if (it != mEntries.end()) {
        if (auto lock = it->second.lock()) {
            return lock;
        }
    }
    auto entry = ClapEntry::Load(path);
    mEntries[path] = entry;
    return entry;
}

PluginPath ClapEffectsProvider::GetPath() const
{
    return {};
}

ComponentInterfaceSymbol ClapEffectsProvider::GetSymbol() const
{
    return XO("CLAP Effects");
}

VendorSymbol ClapEffectsProvider::GetVendor() const
{
    return XO("The Audacity Team");
}

wxString ClapEffectsProvider::GetVersion() const
{
    return AUDACITY_VERSION_STRING;
}

TranslatableString ClapEffectsProvider::GetDescription() const
{
    return XO("Adds the ability to use CLAP effects in Audacity.");
}

bool ClapEffectsProvider::Initialize()
{
    return true;
}

void ClapEffectsProvider::Terminate()
{
}

EffectFamilySymbol ClapEffectsProvider::GetOptionalFamilySymbol()
{
    return ClapEffectBase::GetFamilySymbol();
}

const FileExtensions& ClapEffectsProvider::GetFileExtensions()
{
    static const FileExtensions ext { { _T("clap") } };
    return ext;
}

FilePath ClapEffectsProvider::InstallPath()
{
    return {};
}

void ClapEffectsProvider::AutoRegisterPlugins(PluginManagerInterface&)
{
}

bool ClapEffectsProvider::SupportsCustomModulePaths() const
{
    return true;
}

PluginPaths ClapEffectsProvider::FindModulePaths(PluginManagerInterface& pluginManager,
                                               BasicUI::ProgressDialog* progress) const
{
    FilePaths pathList;
    appendStandardPaths(pathList);

    // Application-bundled "clap" folder, next to the executable.
    {
        auto path = wxFileName(PlatformCompatibility::GetExecutablePath());
        path.AppendDir(wxT("clap"));
        pathList.push_back(path.GetPath());
    }

    // User-configured custom paths.
    {
        auto customPaths = pluginManager.ReadCustomPaths(*this);
        std::copy(customPaths.begin(), customPaths.end(), std::back_inserter(pathList));
    }

    PluginPaths result;
    for (const auto& dirPath : pathList) {
        ClapPluginTraverser traverser([&](const wxString& pluginPath){
            result.push_back(pluginPath);
        }, progress);
        wxDir dir(dirPath);
        if (dir.IsOpened()) {
            dir.Traverse(traverser, wxEmptyString, wxDIR_DEFAULT);
        }
        if (traverser.cancelled()) {
            break;
        }
    }
    return result;
}

unsigned ClapEffectsProvider::DiscoverPluginsAtPath(const PluginPath& path, TranslatableString& errMsg,
                                                  const RegistrationCallback& callback)
{
    try {
        wxString modulePath;
        ClapUtils::ParsePluginPath(path, &modulePath, nullptr);

        auto entry = GetEntry(modulePath);
        const auto* factory = entry->factory();

        unsigned nEffects = 0;
        const uint32_t count = factory->get_plugin_count(factory);
        for (uint32_t i = 0; i < count; ++i) {
            const auto* desc = factory->get_plugin_descriptor(factory, i);
            if (!desc || !desc->id) {
                continue;
            }
            if (!ClapUtils::IsHostableEffect(desc->features)) {
                continue;
            }
            try {
                auto effect = std::make_unique<ClapEffectBase>(entry, *desc);
                ++nEffects;
                if (callback) {
                    callback(this, effect.get());
                }
            } catch (const std::exception& e) {
                wxLogError("CLAP effect %s@%s cannot be loaded: %s", desc->id, modulePath, e.what());
            }
        }

        if (nEffects == 0) {
            throw std::runtime_error("no effects found");
        }
        return nEffects;
    } catch (const std::exception& e) {
        errMsg = XO("CLAP module error: %s").Format(e.what());
    }
    return 0u;
}

bool ClapEffectsProvider::CheckPluginExist(const PluginPath& path) const
{
    wxString modulePath;
    if (ClapUtils::ParsePluginPath(path, &modulePath, nullptr)) {
        return wxFileName::FileExists(modulePath) || wxFileName::DirExists(modulePath);
    }
    return wxFileName::FileExists(path) || wxFileName::DirExists(path);
}

std::unique_ptr<ComponentInterface> ClapEffectsProvider::LoadPlugin(const PluginPath& path)
{
    try {
        wxString modulePath;
        std::string pluginId;
        if (!ClapUtils::ParsePluginPath(path, &modulePath, &pluginId)) {
            throw std::runtime_error("failed to parse plugin path");
        }

        auto entry = GetEntry(modulePath);
        const auto* factory = entry->factory();
        const uint32_t count = factory->get_plugin_count(factory);
        for (uint32_t i = 0; i < count; ++i) {
            const auto* desc = factory->get_plugin_descriptor(factory, i);
            if (desc && desc->id && pluginId == desc->id) {
                return std::make_unique<ClapEffectBase>(entry, *desc);
            }
        }
        throw std::runtime_error("plugin id not found");
    } catch (const std::exception& e) {
        wxLogError("CLAP module was not loaded: %s", e.what());
    }
    return nullptr;
}

namespace {
class ClapPluginValidator final : public PluginProvider::Validator
{
public:
    void Validate(ComponentInterface& component) override
    {
        if (auto* effect = dynamic_cast<ClapEffectBase*>(&component)) {
            ClapWrapper wrapper(effect->entry(), effect->pluginId());
            wrapper.InitializeComponents();
        } else {
            throw std::runtime_error("Not a ClapEffect");
        }
    }
};
}

std::unique_ptr<PluginProvider::Validator> ClapEffectsProvider::MakeValidator() const
{
    return std::make_unique<ClapPluginValidator>();
}
