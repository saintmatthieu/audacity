#include "amplify.h"
#include "libraries/lib-effects/LoadEffects.h"

using namespace au::effects;

const ComponentInterfaceSymbol Amplify::Symbol { XO("Amplify") };

namespace {
BuiltinEffectsModule::Registration<Amplify> reg;
}

void Amplify::ForceLinkage()
{
}

ComponentInterfaceSymbol Amplify::GetSymbol() const
{
    return Symbol;
}

bool Amplify::IsInteractive() const
{
    return false;
}

std::shared_ptr<EffectInstance> Amplify::MakeInstance() const
{
    return std::make_shared<StatefulPerTrackEffect::Instance>(const_cast<Amplify&>(*this));
}
