/*  SPDX-License-Identifier: GPL-2.0-or-later */
/*!********************************************************************

  Audacity: A Digital Audio Editor

  DoEffect.cpp

  Matthieu Hodgkinson

**********************************************************************/
#include "DoEffect.h"
#include "AudacityMessageBox.h"
#include "Effect.h"
#include "GetMessageBoxCb.h"
#include "AudacityApplicationLogic.h"
#include "ProjectAudioManager.h"
#include "ProjectWindows.h"
#include "SelectUtilities.h"
#include "effects/EffectUI.h"
#include "effects/EffectUIServices.h"
#include <wx/frame.h>

// Definition of AudacityApplicationLogic::GetMessageBoxCb(), declared in lib-audacity-application-logic. This
// implementation uses our wxWidgets-based AudacityMessageBox.
AudacityApplicationLogic::MessageBoxCb AudacityApplicationLogic::GetMessageBoxCb()
{
   return
      [](const TranslatableString& message, const TranslatableString& caption) {
         AudacityMessageBox(message, caption);
      };
}

namespace
{
AudacityApplicationLogic::StopPlaybackCb StopPlaybackCb(AudacityProject& project)
{
   return [&]() { ProjectAudioManager::Get(project).Stop(); };
}
} // namespace

bool EffectUI::DoEffect(
   const PluginID& ID, AudacityProject& project, unsigned flags)
{
   auto getShowEffectHostInterfaceCb =
      [window = &GetProjectFrame(project)](
         Effect& effect, std::shared_ptr<EffectInstance>& pInstance,
         SimpleEffectSettingsAccess& access) {
         const auto pServices = dynamic_cast<EffectUIServices*>(&effect);
         return pServices && pServices->ShowHostInterface(
                                effect, *window, EffectUI::DialogFactory,
                                pInstance, access, true);
      };
   auto selectAllIfNoneCb = [&]() {
      SelectUtilities::SelectAllIfNone(project);
   };
   return AudacityApplicationLogic::DoEffect(
      ID, project, flags, std::move(getShowEffectHostInterfaceCb),
      StopPlaybackCb(project), std::move(selectAllIfNoneCb));
}
