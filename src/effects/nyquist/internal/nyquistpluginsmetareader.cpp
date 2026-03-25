/*
* Audacity: A Digital Audio Editor
*/

#include "nyquistpluginsmetareader.h"

#include "effects/effects_base/internal/effectsutils.h"
#include "effects/effects_base/internal/au3/au3effectsutils.h"

#include "au3-module-manager/PluginManager.h"
#include "au3wrap/internal/wxtypes_convert.h"

#include "framework/global/log.h"

using namespace au::effects;
using namespace muse;

void NyquistPluginsMetaReader::init()
{
    // Lazy initialization: if not initialized yet, initialize now
    // This is needed because the subprocess that registers plugins creates new instances
    // of meta readers that haven't been initialized via onInit()
    m_module.Initialize();
}

std::optional<EffectMeta> NyquistPluginsMetaReader::readMeta(const io::path_t& pluginPath) const
{
    try {
        // For Nyquist plugins, we bypass the validation step because they are simple script files
        // We just need to discover them and create metadata without running validation
        auto& module = const_cast<::NyquistEffectsModule&>(m_module);

        std::optional<EffectMeta> meta;
        wxString wxPluginPath = au3::wxFromString(pluginPath.toString());
        ::TranslatableString errorMessage{};

        // Discover the plugin to get its metadata
        int numPlugins = module.DiscoverPluginsAtPath(
            wxPluginPath, errorMessage, [&](PluginProvider* provider, ComponentInterface* ident) -> const PluginID&
        {
            // Use DefaultRegistrationCallback to create the descriptor
            auto& id = PluginManager::DefaultRegistrationCallback(provider, ident);

            if (const auto ptr = PluginManager::Get().GetPlugin(id)) {
                meta = toEffectMeta(*ptr, au3::wxToString(ptr->GetSymbol().Internal()), muse::String {}, true);
            } else {
                LOGW() << "NyquistPluginsMetaReader::readMeta - Could not get plugin descriptor for ID: " << au3::wxToStdString(id);
            }
            return id;
        });

        if (!meta.has_value()) {
            LOGE() << "No Nyquist plugins found at path: " << pluginPath;
        } else {
            assert(numPlugins == 1);
        }

        return meta;
    }
    catch (const std::exception& e) {
        LOGE() << "NyquistPluginsMetaReader::readMeta - Exception: " << e.what();
        return std::nullopt;
    }
    catch (...) {
        LOGE() << "NyquistPluginsMetaReader::readMeta - Unknown exception";
        return std::nullopt;
    }
}
