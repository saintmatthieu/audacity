/*
* Audacity: A Digital Audio Editor
*/
#pragma once

#include "effects/effects_base/internal/au3/au3audiopluginmetareader.h"

#include "au3-clap/ClapEffectsProvider.h"

namespace au::effects {
class ClapPluginsMetaReader : public Au3AudioPluginMetaReader
{
public:
    ClapPluginsMetaReader();
    muse::audio::AudioResourceType metaType() const override;
    bool canReadMeta(const muse::io::path_t& pluginPath) const override;

private:
    ClapEffectsProvider m_module;
};
}
