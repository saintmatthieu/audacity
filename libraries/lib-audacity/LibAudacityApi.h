// TODO header
#pragma once

#include "CommandFlag.h"
#include "LibAudacityTypes.h"
#include "PluginProvider.h"

class AudacityProject;

namespace LibAudacity
{
/** Run an effect given the plugin ID */
// Returns true on success.  Will only operate on tracks that
// have the "selected" flag set to true, which is consistent with
// Audacity's standard UI.
AUDACITY_API bool DoEffect(
   const PluginID& ID, AudacityProject& project, unsigned flags,
   ShowEffectHostInterfaceCb, StopPlaybackCb, SelectAllIfNoneCb);
} // namespace LibAudacity
