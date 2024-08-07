// TODO header
#pragma once

#include "TranslatableString.h"
#include <functional>
#include <memory>

class Effect;
class EffectInstance;
class SimpleEffectSettingsAccess;

namespace AudacityApplicationLogic
{
using MessageBoxCb = std::function<void(
   const TranslatableString& message, const TranslatableString& caption)>;

using ShowEffectHostInterfaceCb = std::function<bool(
   Effect&, std::shared_ptr<EffectInstance>&, SimpleEffectSettingsAccess&)>;

using StopPlaybackCb = std::function<void()>;

using SelectAllIfNoneCb = std::function<void()>;
} // namespace AudacityApplicationLogic
