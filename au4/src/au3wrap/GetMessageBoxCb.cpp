/*
 * Audacity: A Digital Audio Editor
 */
#include "GetMessageBoxCb.h"

AudacityApplicationLogic::MessageBoxCb
AudacityApplicationLogic::GetMessageBoxCb()
{
    return [](const TranslatableString& message,
              const TranslatableString& caption) {};
}
