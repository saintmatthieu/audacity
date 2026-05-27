/*
* Audacity: A Digital Audio Editor
*/
#include "clapeffectsmodule.h"

#include "audioplugins/iaudiopluginsscannerregister.h"
#include "audioplugins/iaudiopluginmetareaderregister.h"

#include "effects/effects_base/ieffectloadersregister.h"
#include "effects/effects_base/ieffectviewlaunchregister.h"
#include "effects/effects_base/iparameterextractorregistry.h"

#include "internal/clapeffectloader.h"
#include "internal/clappluginsscanner.h"
#include "internal/clappluginsmetareader.h"
#include "internal/clapparameterextractorservice.h"
#include "internal/clapviewlauncher.h"

using namespace muse;
using namespace au::effects;

static const std::string mname("effects_clap");

ClapEffectsModule::ClapEffectsModule()
    : m_metaReader(std::make_shared<ClapPluginsMetaReader>()),
    m_effectLoader(std::make_shared<ClapEffectLoader>()),
    m_pluginsScanner(std::make_shared<ClapPluginsScanner>())
{
}

std::string ClapEffectsModule::moduleName() const
{
    return mname;
}

void ClapEffectsModule::registerExports()
{
}

void ClapEffectsModule::resolveImports()
{
    auto scannerRegister = globalIoc()->resolve<muse::audioplugins::IAudioPluginsScannerRegister>(mname);
    if (scannerRegister) {
        scannerRegister->registerScanner(m_pluginsScanner);
    }

    auto metaReaderRegister = globalIoc()->resolve<muse::audioplugins::IAudioPluginMetaReaderRegister>(mname);
    if (metaReaderRegister) {
        metaReaderRegister->registerReader(m_metaReader);
    }

    auto paramExtractorRegistry = globalIoc()->resolve<IParameterExtractorRegistry>(mname);
    if (paramExtractorRegistry) {
        paramExtractorRegistry->registerExtractor(std::make_shared<ClapParameterExtractorService>());
    }

    auto loadersRegister = globalIoc()->resolve<IEffectLoadersRegister>(mname);
    if (loadersRegister) {
        loadersRegister->registerLoader(m_effectLoader);
    }
}

void ClapEffectsModule::registerResources()
{
}

void ClapEffectsModule::registerUiTypes()
{
}

void ClapEffectsModule::onInit(const muse::IApplication::RunMode& mode)
{
    m_metaReader->init();
    m_effectLoader->init();
    m_pluginsScanner->init(mode);
}

void ClapEffectsModule::onDeinit()
{
    m_effectLoader->deinit();
    m_pluginsScanner->deinit();
    m_metaReader->deinit();
}

muse::modularity::IContextSetup* ClapEffectsModule::newContext(const muse::modularity::ContextPtr& ctx) const
{
    return new ClapEffectsContext(ctx);
}

// =====================================================
// ClapEffectsContext
// =====================================================

void ClapEffectsContext::registerExports()
{
}

void ClapEffectsContext::resolveImports()
{
    auto lr = ioc()->resolve<IEffectViewLaunchRegister>(mname);
    if (lr) {
        lr->regLauncher(EffectFamily::CLAP, std::make_shared<ClapViewLauncher>(iocContext()));
    }
}

void ClapEffectsContext::onDeinit()
{
}
