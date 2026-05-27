/*
 * Audacity: A Digital Audio Editor
 */
#pragma once

#include "modularity/imodulesetup.h"

namespace au::effects {
class ClapEffectsModule : public muse::modularity::IModuleSetup
{
public:
    ClapEffectsModule() = default;

    std::string moduleName() const override;
};
}
