/**********************************************************************

  Audacity: A Digital Audio Editor

  @file ClapParameterExtraction.cpp

**********************************************************************/
#include "ClapParameterExtraction.h"

#include <cmath>

#include <clap/ext/params.h>

#include "au3-components/EffectInterface.h"

#include "ClapInstance.h"
#include "ClapWrapper.h"

namespace ClapParameterExtraction {
namespace {
ClapWrapper* wrapperFor(EffectInstanceEx* instance)
{
    auto* clapInstance = dynamic_cast<ClapInstance*>(instance);
    return clapInstance ? &clapInstance->GetWrapper() : nullptr;
}

ParamInfo buildParamInfo(ClapWrapper& wrapper, const clap_param_info_t& ci)
{
    ParamInfo info;
    info.id = ci.id;
    info.name = ci.name;
    info.group = ci.module;
    info.minValue = ci.min_value;
    info.maxValue = ci.max_value;
    info.defaultValue = ci.default_value;

    double value = ci.default_value;
    wrapper.GetParameterValue(ci.id, value);
    info.currentValue = value;
    wrapper.ParameterValueToText(ci.id, value, info.currentValueString);

    const bool stepped = (ci.flags & CLAP_PARAM_IS_STEPPED) != 0;
    const bool isEnum = (ci.flags & CLAP_PARAM_IS_ENUM) != 0;
    info.isReadOnly = (ci.flags & CLAP_PARAM_IS_READONLY) != 0;
    info.isHidden = (ci.flags & CLAP_PARAM_IS_HIDDEN) != 0;
    info.canAutomate = (ci.flags & CLAP_PARAM_IS_AUTOMATABLE) != 0;
    info.isInteger = stepped;

    if (isEnum) {
        info.type = ParamType::Dropdown;
        const int lo = static_cast<int>(std::lround(ci.min_value));
        const int hi = static_cast<int>(std::lround(ci.max_value));
        if (hi >= lo && (hi - lo) <= 512) {
            for (int v = lo; v <= hi; ++v) {
                std::string label;
                if (wrapper.ParameterValueToText(ci.id, static_cast<double>(v), label)) {
                    info.enumValues.push_back(label);
                    info.enumIndices.push_back(static_cast<double>(v));
                }
            }
        }
    } else if (info.isReadOnly) {
        info.type = ParamType::ReadOnly;
    } else if (stepped && ci.min_value == 0.0 && ci.max_value == 1.0) {
        info.type = ParamType::Toggle;
    } else if (stepped) {
        info.type = ParamType::Numeric;
        info.stepCount = static_cast<int>(ci.max_value - ci.min_value);
        info.stepSize = 1.0;
    } else {
        info.type = ParamType::Slider;
    }

    return info;
}
}

std::vector<ParamInfo> extractParameters(EffectInstanceEx* instance, EffectSettingsAccess* settingsAccess)
{
    auto* wrapper = wrapperFor(instance);
    if (!wrapper) {
        return {};
    }

    if (settingsAccess) {
        settingsAccess->ModifySettings([wrapper](EffectSettings& settings) {
            wrapper->FetchSettings(settings);
            return nullptr;
        });
        // Deliver the just-fetched values to the (inactive) plugin so get_value reflects them.
        wrapper->FlushParameters();
    }

    std::vector<ParamInfo> result;
    const uint32_t count = wrapper->GetParameterCount();
    result.reserve(count);
    for (uint32_t i = 0; i < count; ++i) {
        clap_param_info_t ci {};
        if (wrapper->GetParameterInfo(i, ci)) {
            result.push_back(buildParamInfo(*wrapper, ci));
        }
    }
    return result;
}

ParamInfo getParameter(EffectInstanceEx* instance, uint32_t parameterId)
{
    auto* wrapper = wrapperFor(instance);
    if (!wrapper) {
        return {};
    }
    const uint32_t count = wrapper->GetParameterCount();
    for (uint32_t i = 0; i < count; ++i) {
        clap_param_info_t ci {};
        if (wrapper->GetParameterInfo(i, ci) && ci.id == parameterId) {
            return buildParamInfo(*wrapper, ci);
        }
    }
    return {};
}

double getParameterValue(EffectInstanceEx* instance, uint32_t parameterId)
{
    auto* wrapper = wrapperFor(instance);
    double value = 0.0;
    if (wrapper) {
        wrapper->GetParameterValue(parameterId, value);
    }
    return value;
}

bool setParameterValue(EffectInstanceEx* instance, uint32_t parameterId, double plainValue,
                       EffectSettingsAccess* settingsAccess)
{
    auto* wrapper = wrapperFor(instance);
    if (!wrapper) {
        return false;
    }
    if (settingsAccess) {
        settingsAccess->ModifySettings([wrapper, parameterId, plainValue](EffectSettings& settings) {
            wrapper->SetParameterValue(parameterId, plainValue, &settings);
            return nullptr;
        });
    } else {
        wrapper->SetParameterValue(parameterId, plainValue, nullptr);
    }
    return true;
}

std::string getParameterValueString(EffectInstanceEx* instance, uint32_t parameterId, double plainValue)
{
    auto* wrapper = wrapperFor(instance);
    std::string text;
    if (wrapper) {
        wrapper->ParameterValueToText(parameterId, plainValue, text);
    }
    return text;
}

void flushAndStoreSettings(EffectInstanceEx* instance, EffectSettingsAccess* settingsAccess)
{
    auto* wrapper = wrapperFor(instance);
    if (!wrapper || !settingsAccess) {
        return;
    }
    settingsAccess->ModifySettings([wrapper](EffectSettings& settings) {
        wrapper->StoreSettings(settings);
        return nullptr;
    });
    settingsAccess->Flush();
}
}
