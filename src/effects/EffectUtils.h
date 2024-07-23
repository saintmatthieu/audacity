// TODO header
#pragma once

#include <functional>
#include <memory>

class AudacityProject;
class Effect;
class EffectInstance;
class SimpleEffectSettingsAccess;

namespace EffectUtils
{
using StuffFn = std::function<bool(
   Effect&, std::shared_ptr<EffectInstance>&, SimpleEffectSettingsAccess&)>;
StuffFn MakeStuffFn(AudacityProject& project);
}
