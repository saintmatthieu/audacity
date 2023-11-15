#pragma once

#include "wxPanelWrapper.h"

class WX_WRAPPERS_API AudacityDontAskAgainMessageDialog : private wxDialogWrapper
{
public:
   AudacityDontAskAgainMessageDialog(
      wxWindow* parent, const TranslatableString& caption,
      const TranslatableString& message);

   bool ShowDialog();
   bool IsChecked() const;

private:
   DECLARE_EVENT_TABLE();

   void OnCheckBoxEvent(wxCommandEvent& evt);
   void OnClose(wxCloseEvent& event);
   void OnKeyDown(wxKeyEvent& event);
   void OnButtonClick(wxCommandEvent& event);

   bool mChecked = false;
   bool mOk = false;
};
