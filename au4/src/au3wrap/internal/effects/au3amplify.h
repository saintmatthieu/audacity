/*
 * Audacity: A Digital Audio Editor
 */
#pragma once

#include "libraries/lib-audacity-application-logic/AmplifyBase.h"

namespace au::au3 {
class Au3Amplify final : public AmplifyBase
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
};
} // namespace au::au3
