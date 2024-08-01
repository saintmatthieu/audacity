// TODO header
#pragma once

#include <wx/string.h>

class AudacityProject;
using PluginID = wxString;

namespace EffectUI
{
bool DoEffect(const PluginID& ID, AudacityProject& project, unsigned flags);
}
