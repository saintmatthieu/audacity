#pragma once

#include "ClientData.h"

#include <optional>
#include <thread>
#include <wx/timer.h>

class AudacityProject;
class wxMessageDialog;

class TempoDetectionDialog final : public ClientData::Base
{
public:
   static TempoDetectionDialog& Get(AudacityProject& project);

   TempoDetectionDialog(AudacityProject& project);
   ~TempoDetectionDialog();

   void Show();

private:
   AudacityProject& mProject;
};
