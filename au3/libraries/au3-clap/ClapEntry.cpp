/**********************************************************************

  Audacity: A Digital Audio Editor

  @file ClapEntry.cpp

**********************************************************************/
#include "ClapEntry.h"

#include <stdexcept>

#include <wx/dynlib.h>
#include <wx/filename.h>

#include <clap/version.h>

namespace {
//! Resolve the binary to load from a ".clap" path.
//! On Linux/Windows a ".clap" is a plain shared object; on macOS it is a bundle
//! whose binary lives in Contents/MacOS/<name>.
wxString resolveBinaryPath(const wxString& path)
{
#ifdef __WXMAC__
    if (wxFileName::DirExists(path)) {
        const wxFileName fn(path);
        // GetName() strips the ".clap" extension, matching the bundle binary name.
        return path + wxT("/Contents/MacOS/") + fn.GetName();
    }
#endif
    return path;
}
}

ClapEntry::ClapEntry(wxString path, std::unique_ptr<wxDynamicLibrary> lib,
                     const clap_plugin_entry_t* entry, const clap_plugin_factory_t* factory)
    : mPath(std::move(path)), mLib(std::move(lib)), mEntry(entry), mFactory(factory)
{
}

ClapEntry::~ClapEntry()
{
    if (mEntry) {
        mEntry->deinit();
    }
    // mLib's destructor unloads the DSO after deinit() has run.
}

std::shared_ptr<ClapEntry> ClapEntry::Load(const wxString& path)
{
    auto lib = std::make_unique<wxDynamicLibrary>();
    if (!lib->Load(resolveBinaryPath(path), wxDL_NOW | wxDL_GLOBAL)) {
        throw std::runtime_error("failed to load CLAP library");
    }

    void* symbol = lib->GetSymbol(wxT("clap_entry"));
    if (!symbol) {
        throw std::runtime_error("clap_entry symbol not found");
    }

    const auto* entry = reinterpret_cast<const clap_plugin_entry_t*>(symbol);
    if (!clap_version_is_compatible(entry->clap_version)) {
        throw std::runtime_error("incompatible CLAP version");
    }

    // init() receives the path to the DSO / bundle, not the inner binary.
    if (!entry->init(path.ToUTF8())) {
        throw std::runtime_error("clap_entry.init() failed");
    }

    const auto* factory
        = static_cast<const clap_plugin_factory_t*>(entry->get_factory(CLAP_PLUGIN_FACTORY_ID));
    if (!factory) {
        entry->deinit();
        throw std::runtime_error("no clap plugin factory");
    }

    // Private constructor: use new + shared_ptr rather than make_shared.
    return std::shared_ptr<ClapEntry>(new ClapEntry(path, std::move(lib), entry, factory));
}
