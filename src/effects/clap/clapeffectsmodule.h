/*
* Audacity: A Digital Audio Editor
*/
#pragma once

#include <memory>

#include "modularity/imodulesetup.h"

namespace au::effects {
class ClapPluginsMetaReader;
class ClapEffectLoader;
class ClapPluginsScanner;

class ClapEffectsModule : public muse::modularity::IModuleSetup
{
public:
    ClapEffectsModule();

    std::string moduleName() const override;
    void registerExports() override;
    void resolveImports() override;
    void registerResources() override;
    void registerUiTypes() override;
    void onInit(const muse::IApplication::RunMode& mode) override;
    void onDeinit() override;

    muse::modularity::IContextSetup* newContext(const muse::modularity::ContextPtr& ctx) const override;

private:
    const std::shared_ptr<ClapPluginsMetaReader> m_metaReader;
    const std::shared_ptr<ClapEffectLoader> m_effectLoader;
    const std::shared_ptr<ClapPluginsScanner> m_pluginsScanner;
};

class ClapEffectsContext : public muse::modularity::IContextSetup
{
public:
    ClapEffectsContext(const muse::modularity::ContextPtr& ctx)
        : muse::modularity::IContextSetup(ctx) {}

    void registerExports() override;
    void resolveImports() override;
    void onDeinit() override;
};
}
