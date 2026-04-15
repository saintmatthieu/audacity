/*
 * Audacity: A Digital Audio Editor
 */
#pragma once

#include "effects/effects_base/iparameterextractorservice.h"

#include "framework/global/modularity/ioc.h"
#include "effects/effects_base/ieffectinstancesregister.h"
#include "effects/effects_base/ieffectsprovider.h"

class LV2EffectBase;

namespace au::effects {
class Lv2ParameterExtractorService : public IParameterExtractorService
{
    muse::GlobalInject<IEffectInstancesRegister> instancesRegister;
    muse::GlobalInject<IEffectsProvider> effectsProvider;

public:
    EffectFamily family() const override { return EffectFamily::LV2; }
    ParameterInfoList extractParameters(EffectInstance* instance, EffectSettingsAccessPtr settingsAccess = nullptr) const override;
    ParameterInfo getParameter(EffectInstance* instance, const muse::String& parameterId) const override;
    double getParameterValue(EffectInstance* instance, const muse::String& parameterId) const override;
    bool setParameterValue(EffectInstance* instance, const muse::String& parameterId, double fullRangeValue,
                           EffectSettingsAccessPtr settingsAccess = nullptr) override;
    muse::String getParameterValueString(EffectInstance* instance, const muse::String& parameterId, double value) const override;

private:
    LV2EffectBase* lv2Effect(EffectInstance* instance) const;
};
}
