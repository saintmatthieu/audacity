/*
 * Audacity: A Digital Audio Editor
 */
#pragma once

#include <unordered_map>

#include "effects/effects_base/iparameterextractorservice.h"

namespace au::effects {
//! Parameter extraction for CLAP plugins, feeding the auto-generated UI.
//! Registered with IParameterExtractorRegistry for EffectFamily::CLAP.
class ClapParameterExtractorService : public IParameterExtractorService
{
public:
    EffectFamily family() const override { return EffectFamily::CLAP; }

    ParameterInfoList extractParameters(EffectInstance* instance, EffectSettingsAccessPtr settingsAccess = nullptr) const override;
    ParameterInfo getParameter(EffectInstance* instance, const muse::String& parameterId) const override;
    double getParameterValue(EffectInstance* instance, const muse::String& parameterId) const override;
    bool setParameterValue(EffectInstance* instance, const muse::String& parameterId, double fullRangeValue,
                           EffectSettingsAccessPtr settingsAccess = nullptr) override;
    muse::String getParameterValueString(EffectInstance* instance, const muse::String& parameterId, double value) const override;

    void endParameterGesture(EffectInstance* instance, const muse::String& parameterId) override;
    void beginParameterEditing(EffectInstance* instance, EffectSettingsAccessPtr settingsAccess) override;
    void endParameterEditing(EffectInstance* instance) override;
    void onInstanceDestroyed(EffectInstance* instance) override;

private:
    //! settingsAccess for the currently open editing session, per instance.
    std::unordered_map<EffectInstance*, EffectSettingsAccessPtr> m_editingSettings;
};
}
