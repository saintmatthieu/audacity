/**********************************************************************

  Audacity: A Digital Audio Editor

  @file ClapUtils.h

  @brief Helpers for the CLAP module that do not expose the CLAP SDK types.

**********************************************************************/
#pragma once

#include <string>

#include <wx/string.h>

#include "au3-components/EffectInterface.h" // EffectType

namespace ClapUtils {
//! Encode a module path and a plugin id into a single Audacity plugin path.
//! A single ".clap" file/bundle may expose several plugins, so the plugin id is
//! appended after a ';' separator (the plugin scanner splits on the first ';').
wxString MakePluginPathString(const wxString& modulePath, const std::string& pluginId);

//! Reverse of MakePluginPathString. Returns false when the string is malformed.
//! \p modulePath and/or \p pluginId may be null when the caller is not interested.
bool ParsePluginPath(const wxString& pluginPath, wxString* modulePath, std::string* pluginId);

//! Map a CLAP descriptor feature list (null-terminated array of C strings) to an
//! Audacity EffectType. Audio effects map to Process, analyzers to Analyze.
EffectType EffectTypeFromFeatures(const char* const* features);

//! True if the feature list marks the plugin as an audio effect or analyzer,
//! i.e. something Audacity can host through the effects framework.
bool IsHostableEffect(const char* const* features);
}
