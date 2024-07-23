// TODO header
#include "EffectUtils.h"
#include "Effect.h"
#include "EffectUI.h"
#include "EffectUIServices.h"
#include "ProjectWindows.h"
#include <wx/frame.h>

EffectUtils::StuffFn EffectUtils::MakeStuffFn(AudacityProject& project)
{
   return [&](
             Effect& effect, std::shared_ptr<EffectInstance>& pInstance,
             SimpleEffectSettingsAccess& access) {
      const auto pServices = dynamic_cast<EffectUIServices*>(&effect);
      return pServices && pServices->ShowHostInterface(
                             effect, GetProjectFrame(project),
                             EffectUI::DialogFactory, pInstance, access, true);
   };
}
