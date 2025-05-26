/*
* Audacity: A Digital Audio Editor
*/
#include "lv2effectsmodule.h"

#include "audioplugins/iaudiopluginsscannerregister.h"
#include "audioplugins/iaudiopluginmetareaderregister.h"

#include "effects/effects_base/ieffectviewlaunchregister.h"

#include "internal/lv2effectsrepository.h"
#include "internal/lv2instancesregister.h"
#include "internal/lv2pluginmetareader.h"
#include "internal/lv2pluginsscanner.h"
#include "internal/lv2viewlauncher.h"

#include "log.h"

#include "libraries/lib-lv2/LoadLV2.h"

using namespace muse;
using namespace au::effects;

static void lv2_init_qrc()
{
    //Q_INIT_RESOURCE(lv2);
}

std::string Lv2EffectsModule::moduleName() const
{
    return "effects_lv2";
}

void Lv2EffectsModule::registerExports()
{
    ioc()->registerExport<ILv2EffectsRepository>(moduleName(), new Lv2EffectsRepository());
    ioc()->registerExport<ILv2InstancesRegister>(moduleName(), new Lv2InstancesRegister());
}

void Lv2EffectsModule::resolveImports()
{
    auto scannerRegister = ioc()->resolve<muse::audioplugins::IAudioPluginsScannerRegister>(moduleName());
    if (scannerRegister) {
        scannerRegister->registerScanner(std::make_shared<Lv2PluginsScanner>());
    }

    auto metaReaderRegister = ioc()->resolve<muse::audioplugins::IAudioPluginMetaReaderRegister>(moduleName());
    if (metaReaderRegister) {
        metaReaderRegister->registerReader(std::make_shared<Lv2PluginMetaReader>());
    }

    auto lr = ioc()->resolve<IEffectViewLaunchRegister>(moduleName());
    if (lr) {
        lr->regLauncher("LV2", std::make_shared<Lv2ViewLauncher>());
    }

    // auto ir = ioc()->resolve<IInteractiveUriRegister>(moduleName());
    // if (ir) {
    //     ir->registerQmlUri(Uri("audacity://effects/lv2_viewer"), "Audacity/Lv2/Lv2ViewerDialog.qml");
    // }
}

void Lv2EffectsModule::registerResources()
{
    lv2_init_qrc();
}

void Lv2EffectsModule::registerUiTypes()
{
}

void Lv2EffectsModule::onInit(const muse::IApplication::RunMode&)
{
    // IF_ASSERT_FAILED(Lv2EffectsModule::Initialize()) {
    //     LOGE() << "Failed to initialize LV2";
    //     return;
    // }
}

void Lv2EffectsModule::onDeinit()
{
    // LV2Symbols::FinalizeGWorld();
}
