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
    return true;
}

std::shared_ptr<EffectInstance> Au3Amplify::MakeInstance() const
{
    return std::make_shared<StatefulPerTrackEffect::Instance>(const_cast<Au3Amplify&>(*this));
}

bool Au3Amplify::Show()
{
   // Here comes the QML interaction, reading `mRatio`, exposing it to the user via UI, and writing it back.
   // At the moment we use this hack for the rest of the PoC.
   if (mRatio != 1.2)
      mRatio = 1.2;
   else
      mRatio = 1 / 1.2;
    return true;
}
