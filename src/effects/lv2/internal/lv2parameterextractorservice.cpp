/*
 * Audacity: A Digital Audio Editor
 */
#include "lv2parameterextractorservice.h"

#include "au3-lv2/LV2EffectBase.h"
#include "au3-effects/StatefulEffectBase.h"

namespace au::effects {
namespace {
LV2EffectBase* getEffect(EffectInstance* instance)
{
    if (!instance) {
        return nullptr;
    }
    // The instance is a StatefulEffectBase::Instance, not the LV2EffectBase itself
    // We need to get the effect from the instance
    auto* statefulInstance = dynamic_cast<StatefulEffectBase::Instance*>(instance);
    if (!statefulInstance) {
        return nullptr;
    }

    // Get the effect and cast to LV2EffectBase
    return dynamic_cast<LV2EffectBase*>(&statefulInstance->GetEffect());
}
}

ParameterInfoList Lv2ParameterExtractorService::extractParameters(EffectInstance* instance,
                                                                  [[maybe_unused]] EffectSettingsAccessPtr settingsAccess) const
{
    ParameterInfoList result;
    return result;
}

ParameterInfo Lv2ParameterExtractorService::getParameter(EffectInstance* instance, const muse::String& parameterId) const
{
    return {};
}

double Lv2ParameterExtractorService::getParameterValue(EffectInstance* instance, const muse::String& parameterId) const
{
    return 0.0;
}

bool Lv2ParameterExtractorService::setParameterValue(EffectInstance* instance, const muse::String& parameterId,
                                                     double fullRangeValue, EffectSettingsAccessPtr settingsAccess)
{
    return true;
}

bool Lv2ParameterExtractorService::setParameterStringValue(EffectInstance* instance, const muse::String& parameterId,
                                                           const muse::String& stringValue, EffectSettingsAccessPtr settingsAccess)
{
    return true;
}

muse::String Lv2ParameterExtractorService::getParameterValueString(EffectInstance* instance,
                                                                   const muse::String& parameterId, double value) const
{
    return {};
}
}
