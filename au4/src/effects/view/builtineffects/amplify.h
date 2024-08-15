#pragma once

#include "../abstracteffectmodel.h"
#include "libraries/lib-builtin-effects/AmplifyBase.h"

namespace au::effects {
class Amplify final : public AbstractEffectModel, public AmplifyBase {
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
}
