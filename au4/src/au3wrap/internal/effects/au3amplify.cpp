/*
 * Audacity: A Digital Audio Editor
 */
#include "au3amplify.h"
#include "libraries/lib-effects/LoadEffects.h"

using namespace au::au3;

const ComponentInterfaceSymbol Au3Amplify::Symbol { XO("Amplify") };

namespace {
BuiltinEffectsModule::Registration<Au3Amplify> reg;
}

void Au3Amplify::ForceLinkage()
{
}

ComponentInterfaceSymbol Au3Amplify::GetSymbol() const
{
    return Symbol;
}

bool Au3Amplify::IsInteractive() const
{
    return false;
}

std::shared_ptr<EffectInstance> Au3Amplify::MakeInstance() const
{
    return std::make_shared<StatefulPerTrackEffect::Instance>(const_cast<Au3Amplify&>(*this));
}
