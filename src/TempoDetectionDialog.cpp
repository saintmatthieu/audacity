#include "TempoDetectionDialog.h"

#include "BasicUI.h"
#include "Project.h"
#include "ProjectWindows.h"
#include "wxPanelWrapper.h"

#include <chrono>
#include <wx/msw/msgdlg.h>

namespace
{

class TempoChangeDialog : public wxFrame
{
public:
   TempoChangeDialog()
       : wxFrame(nullptr, 123, "Hello")
   {
      SetSize(320, 180);
      mTimer.SetOwner(this);
      mTimer.StartOnce(3000);
   }

   bool ProcessEvent(wxEvent& event)
   {
      if (event.GetEventType() == wxEVT_TIMER)
      {
         Destroy();
         return true;
      }
      return wxFrame::ProcessEvent(event);
   }

private:
   wxTimer mTimer;
};

} // namespace

static const AttachedProjectObjects::RegisteredFactory key {
   [](AudacityProject& project) {
      return std::make_shared<TempoDetectionDialog>(project);
   }
};

TempoDetectionDialog& TempoDetectionDialog::Get(AudacityProject& project)
{
   return project.AttachedObjects::Get<TempoDetectionDialog&>(key);
}

void TempoDetectionDialog::Show()
{
   auto dialog = new TempoChangeDialog();
   dialog->Show();
}

TempoDetectionDialog::TempoDetectionDialog(AudacityProject& project)
    : mProject { project }
{
}

TempoDetectionDialog::~TempoDetectionDialog()
{
}
