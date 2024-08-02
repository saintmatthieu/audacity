/*
 * Audacity: A Digital Audio Editor
 */
#include "au3compressor.h"
#include "libraries/lib-audacity-application-logic/CompressorInstance.h"
#include "libraries/lib-effects/LoadEffects.h"

using namespace au::au3;

const ComponentInterfaceSymbol Au3Compressor::Symbol { XO(
                                                             "Dummy Compressor") };

namespace {
BuiltinEffectsModule::Registration<Au3Compressor> reg;
}

void Au3Compressor::ForceLinkage()
{
}

ComponentInterfaceSymbol Au3Compressor::GetSymbol() const
{
    return Symbol;
}

bool Au3Compressor::IsInteractive() const
{
    return false;
}

std::shared_ptr<EffectInstance> Au3Compressor::MakeInstance() const
{
    return std::make_shared<CompressorInstance>(*this);
}

EffectType Au3Compressor::GetType() const
{
    return EffectTypeProcess;
}
