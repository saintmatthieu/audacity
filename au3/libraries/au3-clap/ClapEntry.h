/**********************************************************************

  Audacity: A Digital Audio Editor

  @file ClapEntry.h

  @brief RAII loader for a .clap shared library / bundle.

  Loads the DSO, validates and initializes the exported `clap_entry` symbol and
  exposes the plugin factory. One ClapEntry corresponds to one loaded ".clap".

**********************************************************************/
#pragma once

#include <memory>
#include <string>

#include <wx/string.h>

#include <clap/entry.h>
#include <clap/factory/plugin-factory.h>

class wxDynamicLibrary;

class ClapEntry final
{
public:
    //! Load and initialize the ".clap" at \p path. Throws std::runtime_error on
    //! any failure (cannot load, incompatible version, missing factory, ...).
    static std::shared_ptr<ClapEntry> Load(const wxString& path);

    ~ClapEntry();

    ClapEntry(const ClapEntry&) = delete;
    ClapEntry& operator=(const ClapEntry&) = delete;

    const clap_plugin_factory_t* factory() const { return mFactory; }
    const wxString& path() const { return mPath; }

private:
    ClapEntry(wxString path, std::unique_ptr<wxDynamicLibrary> lib,
              const clap_plugin_entry_t* entry, const clap_plugin_factory_t* factory);

    wxString mPath;
    std::unique_ptr<wxDynamicLibrary> mLib;
    const clap_plugin_entry_t* mEntry { nullptr };
    const clap_plugin_factory_t* mFactory { nullptr };
};
