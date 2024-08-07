/*
 * Audacity: A Digital Audio Editor
 */
#pragma once

#include "libraries/lib-components/ComponentInterfaceSymbol.h"
#include "libraries/lib-dynamic-range-processor/DynamicRangeProcessorTypes.h"
#include "libraries/lib-effects/PerTrackEffect.h"
#include "ieffectdialog.h"

namespace au::au3 {
class Au3Compressor final : public EffectWithSettings<CompressorSettings, PerTrackEffect>, public IEffectDialog
{
public:
    static const ComponentInterfaceSymbol Symbol;
    static void ForceLinkage();

private:
    // Effect
    ComponentInterfaceSymbol GetSymbol() const override;
    bool IsInteractive() const override;

    // EffectInstanceFactory
    std::shared_ptr<EffectInstance> MakeInstance() const override;

    // EffectDefinitionInterface
    EffectType GetType() const override;

    // IEffectDialog
    bool Show() override;
};
} // namespace au::au3
