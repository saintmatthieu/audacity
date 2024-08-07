/*
 * Audacity: A Digital Audio Editor
 */
#pragma once

#include "libraries/lib-audacity-application-logic/AmplifyBase.h"
#include "ieffectdialog.h"

namespace au::au3 {
class Au3Amplify final : public AmplifyBase, public IEffectDialog
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

    // IEffectDialog
    bool Show() override;
};
} // namespace au::au3
