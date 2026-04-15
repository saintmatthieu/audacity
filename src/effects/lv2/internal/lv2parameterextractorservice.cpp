/*
 * Audacity: A Digital Audio Editor
 */
#include "lv2parameterextractorservice.h"

#include "au3wrap/internal/wxtypes_convert.h"

#include "au3-lv2/LV2EffectBase.h"
#include "au3-lv2/LV2Ports.h"

using namespace au::effects;
using namespace muse;

namespace {
//! Find a control port by symbol (the unique identifier for LV2 ports)
//! Returns the index into mControlPorts, or -1 if not found
int findControlPortIndex(const LV2Ports& ports, const String& parameterId)
{
    const std::string sym = parameterId.toStdString();
    const auto& controlPorts = ports.mControlPorts;
    for (size_t i = 0; i < controlPorts.size(); ++i) {
        if (controlPorts[i]->mSymbol.ToStdString() == sym) {
            return static_cast<int>(i);
        }
    }
    return -1;
}

//! Determine the ParameterType from an LV2ControlPort
ParameterType getParameterType(const LV2ControlPort& port)
{
    if (!port.mIsInput) {
        return ParameterType::ReadOnly;
    }
    if (port.mToggle) {
        return ParameterType::Toggle;
    }
    if (port.mEnumeration) {
        return ParameterType::Dropdown;
    }
    if (port.mInteger) {
        return ParameterType::Numeric;
    }
    return ParameterType::Slider;
}

//! Convert an LV2ControlPort + current value into a ParameterInfo
ParameterInfo convertPort(const LV2ControlPort& port, float currentValue)
{
    ParameterInfo info;

    info.id = au::au3::wxToString(port.mSymbol);
    info.name = au::au3::wxToString(port.mName);
    info.units = au::au3::wxToString(port.mUnits);
    info.group = au::au3::wxToString(port.mGroup.Translation());

    info.type = getParameterType(port);

    info.minValue = port.mMin;
    info.maxValue = port.mMax;
    info.defaultValue = port.mDef;
    info.currentValue = currentValue;

    info.isReadOnly = !port.mIsInput;
    info.isLogarithmic = port.mLogarithmic;
    info.isInteger = port.mInteger;
    info.isHidden = port.mTrigger;

    if (port.mToggle) {
        info.stepCount = 1;
    } else if (port.mEnumeration && !port.mScaleValues.empty()) {
        info.enumValues.reserve(port.mScaleLabels.size());
        info.enumIndices.reserve(port.mScaleValues.size());
        for (size_t i = 0; i < port.mScaleLabels.size(); ++i) {
            info.enumValues.push_back(au::au3::wxToString(port.mScaleLabels[i]));
        }
        for (const auto& v : port.mScaleValues) {
            info.enumIndices.push_back(v);
        }
        info.stepCount = static_cast<int>(port.mScaleValues.size()) - 1;
    } else if (port.mInteger) {
        info.stepSize = 1.0;
    }

    // Format current value string
    if (port.mToggle) {
        info.currentValueString = currentValue > 0 ? u"On" : u"Off";
    } else if (port.mEnumeration && !port.mScaleLabels.empty()) {
        auto idx = port.Discretize(currentValue);
        if (idx < port.mScaleLabels.size()) {
            info.currentValueString = au::au3::wxToString(port.mScaleLabels[idx]);
        }
    } else if (port.mInteger) {
        info.currentValueString = String::number(static_cast<int>(currentValue));
    } else {
        info.currentValueString = String::number(static_cast<double>(currentValue), 2);
    }

    return info;
}
} // anonymous namespace

//! Get LV2EffectBase from the effect registry, given an EffectInstance
LV2EffectBase* Lv2ParameterExtractorService::lv2Effect(au::effects::EffectInstance* instance) const
{
    if (!instance) {
        return nullptr;
    }
    const EffectInstanceId instanceId = instance->id();
    const EffectId effectId = instancesRegister()->effectIdByInstanceId(instanceId);
    if (effectId.empty()) {
        return nullptr;
    }
    return dynamic_cast<LV2EffectBase*>(effectsProvider()->effect(effectId));
}

ParameterInfoList Lv2ParameterExtractorService::extractParameters(EffectInstance* instance,
                                                                   EffectSettingsAccessPtr settingsAccess) const
{
    LV2EffectBase* effect = lv2Effect(instance);
    if (!effect) {
        return {};
    }

    const auto& controlPorts = effect->mPorts.mControlPorts;

    // Read current values from settings
    std::vector<float> values;
    if (settingsAccess) {
        const auto& settings = settingsAccess->Get();
        values = GetSettings(settings).values;
    }

    ParameterInfoList result;
    result.reserve(controlPorts.size());
    for (size_t i = 0; i < controlPorts.size(); ++i) {
        const auto& port = *controlPorts[i];
        float value = (i < values.size()) ? values[i] : port.mDef;
        result.push_back(convertPort(port, value));
    }
    return result;
}

ParameterInfo Lv2ParameterExtractorService::getParameter(EffectInstance* instance, const String& parameterId) const
{
    LV2EffectBase* effect = lv2Effect(instance);
    if (!effect) {
        return {};
    }

    const int idx = findControlPortIndex(effect->mPorts, parameterId);
    if (idx < 0) {
        return {};
    }

    // Read current value from settings
    float value = effect->mPorts.mControlPorts[idx]->mDef;
    EffectSettingsAccessPtr settingsAccess = instancesRegister()->settingsAccessById(instance->id());
    if (settingsAccess) {
        const auto& settings = settingsAccess->Get();
        const auto& values = GetSettings(settings).values;
        if (static_cast<size_t>(idx) < values.size()) {
            value = values[idx];
        }
    }

    return convertPort(*effect->mPorts.mControlPorts[idx], value);
}

double Lv2ParameterExtractorService::getParameterValue(EffectInstance* instance, const String& parameterId) const
{
    LV2EffectBase* effect = lv2Effect(instance);
    if (!effect) {
        return 0.0;
    }

    const int idx = findControlPortIndex(effect->mPorts, parameterId);
    if (idx < 0) {
        return 0.0;
    }

    EffectSettingsAccessPtr settingsAccess = instancesRegister()->settingsAccessById(instance->id());
    if (settingsAccess) {
        const auto& settings = settingsAccess->Get();
        const auto& values = GetSettings(settings).values;
        if (static_cast<size_t>(idx) < values.size()) {
            return values[idx];
        }
    }

    return effect->mPorts.mControlPorts[idx]->mDef;
}

bool Lv2ParameterExtractorService::setParameterValue(EffectInstance* instance, const String& parameterId,
                                                      double fullRangeValue, EffectSettingsAccessPtr settingsAccess)
{
    LV2EffectBase* effect = lv2Effect(instance);
    if (!effect) {
        return false;
    }

    const int idx = findControlPortIndex(effect->mPorts, parameterId);
    if (idx < 0) {
        return false;
    }

    const auto& port = *effect->mPorts.mControlPorts[idx];
    if (!port.mIsInput) {
        return false;
    }

    // Clamp to port range
    float clamped = static_cast<float>(
        std::max(static_cast<double>(port.mMin),
                 std::min(static_cast<double>(port.mMax), fullRangeValue)));

    if (!settingsAccess) {
        settingsAccess = instancesRegister()->settingsAccessById(instance->id());
    }
    if (!settingsAccess) {
        return false;
    }

    settingsAccess->ModifySettings([&](EffectSettings& settings) {
        auto& values = GetSettings(settings).values;
        if (static_cast<size_t>(idx) < values.size()) {
            values[idx] = clamped;
        }
        return nullptr;
    });

    return true;
}

String Lv2ParameterExtractorService::getParameterValueString(EffectInstance* instance,
                                                              const String& parameterId, double value) const
{
    LV2EffectBase* effect = lv2Effect(instance);
    if (!effect) {
        return {};
    }

    const int idx = findControlPortIndex(effect->mPorts, parameterId);
    if (idx < 0) {
        return {};
    }

    const auto& port = *effect->mPorts.mControlPorts[idx];

    if (port.mToggle) {
        return value > 0 ? u"On" : u"Off";
    }
    if (port.mEnumeration && !port.mScaleLabels.empty()) {
        auto s = port.Discretize(static_cast<float>(value));
        if (s < port.mScaleLabels.size()) {
            return au::au3::wxToString(port.mScaleLabels[s]);
        }
    }
    if (port.mInteger) {
        return String::number(static_cast<int>(value));
    }
    return String::number(value, 2);
}
