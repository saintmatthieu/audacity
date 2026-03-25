/*
* Audacity: A Digital Audio Editor
*/
#pragma once

#include "effects/effects_base/effectstypes.h"

#include "au3-nyquist-effects/LoadNyquist.h"

namespace au::effects {
class NyquistPluginsMetaReader
{
public:
    void init();
    std::optional<EffectMeta> readMeta(const muse::io::path_t& pluginPath) const;

private:
    ::NyquistEffectsModule m_module;
};
}
