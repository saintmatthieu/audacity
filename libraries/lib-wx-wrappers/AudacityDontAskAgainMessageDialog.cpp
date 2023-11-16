#include "AudacityDontAskAgainMessageDialog.h"

#include <wx/checkbox.h>
#include <wx/sizer.h>
#include <wx/stattext.h>

namespace
{
constexpr auto style = wxDEFAULT_DIALOG_STYLE | wxCENTRE;
}

BEGIN_EVENT_TABLE(AudacityDontAskAgainMessageDialog, wxDialogWrapper)
EVT_CHECKBOX(wxID_ANY, AudacityDontAskAgainMessageDialog::OnCheckBoxEvent)
EVT_CLOSE(AudacityDontAskAgainMessageDialog::OnClose)
END_EVENT_TABLE()

AudacityDontAskAgainMessageDialog::AudacityDontAskAgainMessageDialog(
   wxWindow* parent, const TranslatableString& caption,
   const TranslatableString& message)
    // standard XO("Dialog") could be a good name for the dialog
    : wxDialogWrapper(
         parent, wxID_ANY, caption, wxDefaultPosition, wxDefaultSize, style)
{
   wxStaticText* messageText =
      new wxStaticText(this, wxID_ANY, message.Translation());

   // Add a checkbox
   wxCheckBox* checkBox =
      new wxCheckBox(this, wxID_ANY, XO("Don't ask me again").Translation());

   // Add sizers to arrange controls
   wxBoxSizer* mainSizer = new wxBoxSizer(wxVERTICAL);
   mainSizer->Add(messageText, 0, wxALL | wxALIGN_CENTER, 5);
   mainSizer->Add(checkBox, 0, wxALL | wxALIGN_CENTER, 5);

   // This is where you specified wxOK | wxCANCEL buttons which you saw in your
   // dialog wxOK had no effect because this flag isn't handled by the wxDialog,
   // but even if it did, then you would likely got two rows of buttons
   wxStdDialogButtonSizer* buttonSizer =
      CreateStdDialogButtonSizer(wxYES | wxNO);
   mainSizer->Add(buttonSizer, 0, wxALL | wxALIGN_CENTER, 5);

   SetSizerAndFit(mainSizer);
   // Manually implement wxCENTRE flag behavior
   if (style | wxCENTRE != 0)
      CentreOnParent();

   SetEscapeId(wxID_NO);
}

bool AudacityDontAskAgainMessageDialog::ShowDialog()
{
   return ShowModal() == wxID_YES;
}

bool AudacityDontAskAgainMessageDialog::IsChecked() const
{
   return mChecked;
}

void AudacityDontAskAgainMessageDialog::OnCheckBoxEvent(wxCommandEvent& evt)
{
   mChecked = evt.IsChecked();
}

void AudacityDontAskAgainMessageDialog::OnClose(wxCloseEvent& event)
{
   EndModal(wxID_NO);
}
