/**********************************************************************

  Audacity: A Digital Audio Editor

  @file ClapUtils.cpp

**********************************************************************/
#include "ClapUtils.h"

#include <cstring>

#include <clap/plugin-features.h>

namespace ClapUtils {
wxString MakePluginPathString(const wxString& modulePath, const std::string& pluginId)
{
    // ';' matches the separator the plugin scanner uses to recover the module path.
    return modulePath + wxT(";") + wxString::FromUTF8(pluginId);
}

bool ParsePluginPath(const wxString& pluginPath, wxString* modulePath, std::string* pluginId)
{
    const auto sep = pluginPath.Find(';');
    if (sep == wxNOT_FOUND) {
        // No id appended: the whole string is the module path.
        if (modulePath) {
            *modulePath = pluginPath;
        }
        if (pluginId) {
            pluginId->clear();
        }
        return !pluginPath.empty();
    }

    if (modulePath) {
        *modulePath = pluginPath.Left(sep);
    }
    if (pluginId) {
        *pluginId = pluginPath.Mid(sep + 1).ToStdString();
    }
    return true;
}

namespace {
bool hasFeature(const char* const* features, const char* feature)
{
    if (!features) {
        return false;
    }
    for (auto f = features; *f; ++f) {
        if (std::strcmp(*f, feature) == 0) {
            return true;
        }
    }
    return false;
}
}

EffectType EffectTypeFromFeatures(const char* const* features)
{
    // Instruments and note effects are note-driven and not yet supported; treat
    // them as "none" so they are filtered out during discovery.
    if (hasFeature(features, CLAP_PLUGIN_FEATURE_INSTRUMENT)
        || hasFeature(features, CLAP_PLUGIN_FEATURE_NOTE_EFFECT)
        || hasFeature(features, CLAP_PLUGIN_FEATURE_NOTE_DETECTOR)) {
        return EffectTypeNone;
    }
    if (hasFeature(features, CLAP_PLUGIN_FEATURE_ANALYZER)) {
        return EffectTypeAnalyze;
    }
    if (hasFeature(features, CLAP_PLUGIN_FEATURE_AUDIO_EFFECT)) {
        return EffectTypeProcess;
    }
    return EffectTypeNone;
}

bool IsHostableEffect(const char* const* features)
{
    const auto type = EffectTypeFromFeatures(features);
    return type == EffectTypeProcess || type == EffectTypeAnalyze;
}
}
