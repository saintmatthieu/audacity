/*
* Audacity: A Digital Audio Editor
*/
#pragma once

#include "effects/effects_base/internal/au3/au3audiopluginscanner.h"

#include "au3-clap/ClapEffectsProvider.h"

namespace au::effects {
class ClapPluginsScanner : public Au3AudioPluginScanner
{
public:
    ClapPluginsScanner()
        : Au3AudioPluginScanner(m_clapModule) {}

protected:
    // Phase 1: only the standard CLAP locations (e.g. ~/.clap) are scanned.
    // User-configurable custom paths are a follow-up.
    muse::io::paths_t customPaths() const override { return {}; }

private:
    ClapEffectsProvider m_clapModule;
};
}
