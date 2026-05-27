/*
* Audacity: A Digital Audio Editor
*/
#include "clappluginsmetareader.h"

using namespace au::effects;
using namespace muse;

ClapPluginsMetaReader::ClapPluginsMetaReader()
    : Au3AudioPluginMetaReader{m_module}
{
}

bool ClapPluginsMetaReader::canReadMeta(const io::path_t& pluginPath) const
{
    return io::suffix(pluginPath) == "clap";
}

audio::AudioResourceType ClapPluginsMetaReader::metaType() const
{
    return audio::AudioResourceType::ClapPlugin;
}
