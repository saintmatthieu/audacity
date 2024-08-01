// TODO header
#include "LibAudacityHelpers.h"
#include "AudacityMessageBox.h"
#include "Effect.h"
#include "GetMessageBoxCb.h"
#include "LibAudacityApi.h"
#include "ProjectAudioManager.h"
#include "ProjectWindows.h"
#include "SelectUtilities.h"
#include "effects/EffectUI.h"
#include "effects/EffectUIServices.h"
#include <wx/frame.h>

// Definition of LibAudacity::GetMessageBoxCb(), declared in lib-audacity. This
// implementation uses our wxWidgets-based AudacityMessageBox.
LibAudacity::MessageBoxCb LibAudacity::GetMessageBoxCb()
{
   return
      [](const TranslatableString& message, const TranslatableString& caption) {
         AudacityMessageBox(message, caption);
      };
}

namespace
{
LibAudacity::StopPlaybackCb StopPlaybackCb(AudacityProject& project)
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
   return LibAudacity::DoEffect(
      ID, project, flags, std::move(getShowEffectHostInterfaceCb),
      StopPlaybackCb(project), std::move(selectAllIfNoneCb));
}
