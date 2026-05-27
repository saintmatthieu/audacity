/*
 * Audacity: A Digital Audio Editor
 */
#include "clapparameterextractorservice.h"

#include "au3-clap/ClapParameterExtraction.h"

using namespace au::effects;
using namespace muse;

namespace {
ParameterType convertType(ClapParameterExtraction::ParamType type)
{
    switch (type) {
    case ClapParameterExtraction::ParamType::Toggle: return ParameterType::Toggle;
    case ClapParameterExtraction::ParamType::Dropdown: return ParameterType::Dropdown;
    case ClapParameterExtraction::ParamType::Slider: return ParameterType::Slider;
    case ClapParameterExtraction::ParamType::Numeric: return ParameterType::Numeric;
    case ClapParameterExtraction::ParamType::ReadOnly: return ParameterType::ReadOnly;
    case ClapParameterExtraction::ParamType::Unknown:
    default:
        return ParameterType::Unknown;
    }
}

ParameterInfo convertInfo(const ClapParameterExtraction::ParamInfo& src)
{
    ParameterInfo info;
    info.id = String::number(static_cast<uint64_t>(src.id));
    info.name = String::fromStdString(src.name);
    info.group = String::fromStdString(src.group);
    info.type = convertType(src.type);

    info.minValue = src.minValue;
    info.maxValue = src.maxValue;
    info.defaultValue = src.defaultValue;
    info.currentValue = src.currentValue;
    info.currentValueString = String::fromStdString(src.currentValueString);

    info.stepCount = src.stepCount;
    info.stepSize = src.stepSize;

    info.enumValues.reserve(src.enumValues.size());
    for (const auto& v : src.enumValues) {
        info.enumValues.push_back(String::fromStdString(v));
    }
    info.enumIndices = src.enumIndices;

    info.isReadOnly = src.isReadOnly;
    info.isHidden = src.isHidden;
    info.isInteger = src.isInteger;
    info.canAutomate = src.canAutomate;
    return info;
}

uint32_t toClapId(const muse::String& parameterId)
{
    const std::string s = parameterId.toStdString();
    try {
        return static_cast<uint32_t>(std::stoul(s));
    } catch (...) {
        return 0;
    }
}
}

ParameterInfoList ClapParameterExtractorService::extractParameters(EffectInstance* instance,
                                                                   EffectSettingsAccessPtr settingsAccess) const
{
    const auto au3Params = ClapParameterExtraction::extractParameters(instance, settingsAccess.get());
    ParameterInfoList result;
    result.reserve(au3Params.size());
    for (const auto& p : au3Params) {
        result.push_back(convertInfo(p));
    }
    return result;
}

ParameterInfo ClapParameterExtractorService::getParameter(EffectInstance* instance, const muse::String& parameterId) const
{
    return convertInfo(ClapParameterExtraction::getParameter(instance, toClapId(parameterId)));
}

double ClapParameterExtractorService::getParameterValue(EffectInstance* instance, const muse::String& parameterId) const
{
    return ClapParameterExtraction::getParameterValue(instance, toClapId(parameterId));
}

bool ClapParameterExtractorService::setParameterValue(EffectInstance* instance, const muse::String& parameterId,
                                                      double fullRangeValue, EffectSettingsAccessPtr settingsAccess)
{
    return ClapParameterExtraction::setParameterValue(instance, toClapId(parameterId), fullRangeValue, settingsAccess.get());
}

muse::String ClapParameterExtractorService::getParameterValueString(EffectInstance* instance, const muse::String& parameterId,
                                                                    double value) const
{
    return String::fromStdString(ClapParameterExtraction::getParameterValueString(instance, toClapId(parameterId), value));
}

void ClapParameterExtractorService::endParameterGesture(EffectInstance* instance, const muse::String&)
{
    const auto it = m_editingSettings.find(instance);
    if (it != m_editingSettings.end()) {
        ClapParameterExtraction::flushAndStoreSettings(instance, it->second.get());
    }
}

void ClapParameterExtractorService::beginParameterEditing(EffectInstance* instance, EffectSettingsAccessPtr settingsAccess)
{
    m_editingSettings[instance] = settingsAccess;
}

void ClapParameterExtractorService::endParameterEditing(EffectInstance* instance)
{
    m_editingSettings.erase(instance);
}

void ClapParameterExtractorService::onInstanceDestroyed(EffectInstance* instance)
{
    m_editingSettings.erase(instance);
}
