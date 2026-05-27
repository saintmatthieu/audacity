/**********************************************************************

  Audacity: A Digital Audio Editor

  @file ClapParameterExtraction.h

  @brief Bridge for reading/writing CLAP parameters without exposing the SDK.

  Mirrors VST3ParameterExtraction. CLAP parameters are expressed in plain
  (display / "full range") values, so no normalization is performed here.

**********************************************************************/
#pragma once

#include <cstdint>
#include <string>
#include <vector>

class EffectInstanceEx;
class EffectSettingsAccess;

namespace ClapParameterExtraction {
//! Mirrors AU4's ParameterType subset relevant to CLAP parameters.
enum class ParamType {
    Unknown = -1,
    Toggle,
    Dropdown,
    Slider,
    Numeric,
    ReadOnly,
};

//! Parameter description in plain (full-range) values.
struct ParamInfo {
    uint32_t id = 0;
    std::string name;
    std::string group;

    ParamType type = ParamType::Unknown;

    double minValue = 0.0;
    double maxValue = 0.0;
    double defaultValue = 0.0;
    double currentValue = 0.0;

    std::string currentValueString;

    int stepCount = 0;
    double stepSize = 0.0;

    std::vector<std::string> enumValues;
    std::vector<double> enumIndices;

    bool isReadOnly = false;
    bool isHidden = false;
    bool isInteger = false;
    bool canAutomate = true;
};

//! Extract all parameters. When \p settingsAccess is provided, the persisted
//! settings are applied to the plugin first so values reflect the stored state.
std::vector<ParamInfo> extractParameters(EffectInstanceEx* instance, EffectSettingsAccess* settingsAccess = nullptr);

//! Single parameter by CLAP id; returns ParamInfo with id == 0 if not found.
ParamInfo getParameter(EffectInstanceEx* instance, uint32_t parameterId);

//! Current plain value of a parameter (0 on error).
double getParameterValue(EffectInstanceEx* instance, uint32_t parameterId);

//! Set a parameter to a plain value, persisting it through \p settingsAccess.
bool setParameterValue(EffectInstanceEx* instance, uint32_t parameterId, double plainValue,
                       EffectSettingsAccess* settingsAccess = nullptr);

//! Formatted plugin string for a plain value (empty on error).
std::string getParameterValueString(EffectInstanceEx* instance, uint32_t parameterId, double plainValue);

//! Persist the current plugin state into settings (e.g. at the end of a gesture).
void flushAndStoreSettings(EffectInstanceEx* instance, EffectSettingsAccess* settingsAccess);
} // namespace ClapParameterExtraction
