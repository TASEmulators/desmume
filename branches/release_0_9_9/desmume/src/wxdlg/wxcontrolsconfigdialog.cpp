/////////////////////////////////////////////////////////////////////////////
// Name:        wxcontrolsconfigdialog.cpp
// Purpose:     
// Author:      
// Modified by: 
// Created:     09/07/2010 15:27:00
// RCS-ID:      
// Copyright:   
// Licence:     
/////////////////////////////////////////////////////////////////////////////

// For compilers that support precompilation, includes "wx/wx.h".
#include "wx/wxprec.h"

#ifdef __BORLANDC__
#pragma hdrstop
#endif

#ifndef WX_PRECOMP
#include "wx/wx.h"
#endif

////@begin includes
////@end includes

#include "wxcontrolsconfigdialog.h"

////@begin XPM images
////@end XPM images


/*
 * wxControlsConfigDialog type definition
 */

IMPLEMENT_DYNAMIC_CLASS( wxControlsConfigDialog, wxDialog )


/*
 * wxControlsConfigDialog event table definition
 */

BEGIN_EVENT_TABLE( wxControlsConfigDialog, wxDialog )

////@begin wxControlsConfigDialog event table entries
    EVT_BUTTON( ID_K_L_C, wxControlsConfigDialog::OnChangeKeyboardMapping )

    EVT_BUTTON( ID_K_R_C, wxControlsConfigDialog::OnChangeKeyboardMapping )

    EVT_BUTTON( ID_K_X_C, wxControlsConfigDialog::OnChangeKeyboardMapping )

    EVT_BUTTON( ID_K_Y_C, wxControlsConfigDialog::OnChangeKeyboardMapping )

    EVT_BUTTON( ID_K_A_C, wxControlsConfigDialog::OnChangeKeyboardMapping )

    EVT_BUTTON( ID_K_B_C, wxControlsConfigDialog::OnChangeKeyboardMapping )

    EVT_BUTTON( ID_K_START_C, wxControlsConfigDialog::OnChangeKeyboardMapping )

    EVT_BUTTON( ID_K_SELECT_C, wxControlsConfigDialog::OnChangeKeyboardMapping )

    EVT_BUTTON( ID_K_UP_C, wxControlsConfigDialog::OnChangeKeyboardMapping )

    EVT_BUTTON( ID_K_DOWN_C, wxControlsConfigDialog::OnChangeKeyboardMapping )

    EVT_BUTTON( ID_K_LEFT_C, wxControlsConfigDialog::OnChangeKeyboardMapping )

    EVT_BUTTON( ID_K_RIGHT_C, wxControlsConfigDialog::OnChangeKeyboardMapping )

    EVT_BUTTON( ID_K_LID_C, wxControlsConfigDialog::OnChangeKeyboardMapping )

    EVT_CHOICE( ID_JOYSTICKIDX, wxControlsConfigDialog::OnJoystickidxSelected )

    EVT_BUTTON( ID_J_L_C, wxControlsConfigDialog::OnChangeJoystickMapping )

    EVT_BUTTON( ID_J_R_C, wxControlsConfigDialog::OnChangeJoystickMapping )

    EVT_BUTTON( ID_J_X_C, wxControlsConfigDialog::OnChangeJoystickMapping )

    EVT_BUTTON( ID_J_Y_C, wxControlsConfigDialog::OnChangeJoystickMapping )

    EVT_BUTTON( ID_J_A_C, wxControlsConfigDialog::OnChangeJoystickMapping )

    EVT_BUTTON( ID_J_B_C, wxControlsConfigDialog::OnChangeJoystickMapping )

    EVT_BUTTON( ID_J_START_C, wxControlsConfigDialog::OnChangeJoystickMapping )

    EVT_BUTTON( ID_J_SELECT_C, wxControlsConfigDialog::OnChangeJoystickMapping )

    EVT_BUTTON( ID_J_LID_C, wxControlsConfigDialog::OnChangeJoystickMapping )

    EVT_RADIOBUTTON( ID_DPAD_OPT1, wxControlsConfigDialog::OnChangeDPadMode )

    EVT_RADIOBUTTON( ID_DPAD_OPT2, wxControlsConfigDialog::OnChangeDPadMode )

    EVT_RADIOBUTTON( ID_DPAD_OPT3, wxControlsConfigDialog::OnChangeDPadMode )

    EVT_BUTTON( wxID_DEFAULT, wxControlsConfigDialog::OnDefaultClick )

    EVT_BUTTON( wxID_OK, wxControlsConfigDialog::OnOkClick )

    EVT_BUTTON( wxID_CANCEL, wxControlsConfigDialog::OnCancelClick )

    EVT_BUTTON( wxID_APPLY, wxControlsConfigDialog::OnApplyClick )

////@end wxControlsConfigDialog event table entries

END_EVENT_TABLE()


/*
 * wxControlsConfigDialog constructors
 */

wxControlsConfigDialog::wxControlsConfigDialog()
{
    Init();
}

wxControlsConfigDialog::wxControlsConfigDialog( wxWindow* parent, wxWindowID id, const wxString& caption, const wxPoint& pos, const wxSize& size, long style )
{
    Init();
    Create(parent, id, caption, pos, size, style);
}


/*
 * wxControlsConfigDialog creator
 */

bool wxControlsConfigDialog::Create( wxWindow* parent, wxWindowID id, const wxString& caption, const wxPoint& pos, const wxSize& size, long style )
{
////@begin wxControlsConfigDialog creation
    SetExtraStyle(wxWS_EX_BLOCK_EVENTS);
    wxDialog::Create( parent, id, caption, pos, size, style );

    CreateControls();
    if (GetSizer())
    {
        GetSizer()->SetSizeHints(this);
    }
    Centre();
////@end wxControlsConfigDialog creation
    return true;
}


/*
 * wxControlsConfigDialog destructor
 */

wxControlsConfigDialog::~wxControlsConfigDialog()
{
////@begin wxControlsConfigDialog destruction
////@end wxControlsConfigDialog destruction
}


/*
 * Member initialisation
 */

void wxControlsConfigDialog::Init()
{
////@begin wxControlsConfigDialog member initialisation
////@end wxControlsConfigDialog member initialisation
}


/*
 * Control creation for wxControlsConfigDialog
 */

void wxControlsConfigDialog::CreateControls()
{    
////@begin wxControlsConfigDialog content construction
    wxControlsConfigDialog* itemDialog1 = this;

    wxBoxSizer* itemBoxSizer2 = new wxBoxSizer(wxVERTICAL);
    itemDialog1->SetSizer(itemBoxSizer2);

    wxBoxSizer* itemBoxSizer3 = new wxBoxSizer(wxHORIZONTAL);
    itemBoxSizer2->Add(itemBoxSizer3, 0, wxALIGN_CENTER_HORIZONTAL|wxALL, 0);

    wxStaticBox* itemStaticBoxSizer4Static = new wxStaticBox(itemDialog1, wxID_ANY, _("Keyboard"));
    wxStaticBoxSizer* itemStaticBoxSizer4 = new wxStaticBoxSizer(itemStaticBoxSizer4Static, wxVERTICAL);
    itemBoxSizer3->Add(itemStaticBoxSizer4, 0, wxALIGN_TOP|wxALL, 5);

    wxFlexGridSizer* itemFlexGridSizer5 = new wxFlexGridSizer(0, 3, 0, 0);
    itemStaticBoxSizer4->Add(itemFlexGridSizer5, 0, wxALIGN_CENTER_HORIZONTAL|wxLEFT|wxRIGHT|wxBOTTOM, 5);

    wxStaticText* itemStaticText6 = new wxStaticText( itemDialog1, wxID_STATIC, _("L:"), wxDefaultPosition, wxDefaultSize, 0 );
    itemFlexGridSizer5->Add(itemStaticText6, 0, wxALIGN_RIGHT|wxALIGN_CENTER_VERTICAL|wxRIGHT, 5);

    wxTextCtrl* itemTextCtrl7 = new wxTextCtrl( itemDialog1, ID_K_L_S, wxEmptyString, wxDefaultPosition, wxDefaultSize, wxTE_READONLY );
    itemFlexGridSizer5->Add(itemTextCtrl7, 0, wxALIGN_CENTER_HORIZONTAL|wxALIGN_CENTER_VERTICAL, 5);

    wxButton* itemButton8 = new wxButton( itemDialog1, ID_K_L_C, _("Change"), wxDefaultPosition, wxDefaultSize, 0 );
    itemFlexGridSizer5->Add(itemButton8, 0, wxALIGN_CENTER_HORIZONTAL|wxALIGN_CENTER_VERTICAL, 5);

    wxStaticText* itemStaticText9 = new wxStaticText( itemDialog1, wxID_STATIC, _("R:"), wxDefaultPosition, wxDefaultSize, 0 );
    itemFlexGridSizer5->Add(itemStaticText9, 0, wxALIGN_RIGHT|wxALIGN_CENTER_VERTICAL|wxRIGHT, 5);

    wxTextCtrl* itemTextCtrl10 = new wxTextCtrl( itemDialog1, ID_K_R_S, wxEmptyString, wxDefaultPosition, wxDefaultSize, wxTE_READONLY );
    itemFlexGridSizer5->Add(itemTextCtrl10, 0, wxALIGN_CENTER_HORIZONTAL|wxALIGN_CENTER_VERTICAL, 5);

    wxButton* itemButton11 = new wxButton( itemDialog1, ID_K_R_C, _("Change"), wxDefaultPosition, wxDefaultSize, 0 );
    itemFlexGridSizer5->Add(itemButton11, 0, wxALIGN_CENTER_HORIZONTAL|wxALIGN_CENTER_VERTICAL, 5);

    wxStaticText* itemStaticText12 = new wxStaticText( itemDialog1, wxID_STATIC, _("X:"), wxDefaultPosition, wxDefaultSize, 0 );
    itemFlexGridSizer5->Add(itemStaticText12, 0, wxALIGN_RIGHT|wxALIGN_CENTER_VERTICAL|wxRIGHT, 5);

    wxTextCtrl* itemTextCtrl13 = new wxTextCtrl( itemDialog1, ID_K_X_S, wxEmptyString, wxDefaultPosition, wxDefaultSize, wxTE_READONLY );
    itemFlexGridSizer5->Add(itemTextCtrl13, 0, wxALIGN_CENTER_HORIZONTAL|wxALIGN_CENTER_VERTICAL, 5);

    wxButton* itemButton14 = new wxButton( itemDialog1, ID_K_X_C, _("Change"), wxDefaultPosition, wxDefaultSize, 0 );
    itemFlexGridSizer5->Add(itemButton14, 0, wxALIGN_CENTER_HORIZONTAL|wxALIGN_CENTER_VERTICAL, 5);

    wxStaticText* itemStaticText15 = new wxStaticText( itemDialog1, wxID_STATIC, _("Y:"), wxDefaultPosition, wxDefaultSize, 0 );
    itemFlexGridSizer5->Add(itemStaticText15, 0, wxALIGN_RIGHT|wxALIGN_CENTER_VERTICAL|wxRIGHT, 5);

    wxTextCtrl* itemTextCtrl16 = new wxTextCtrl( itemDialog1, ID_K_Y_S, wxEmptyString, wxDefaultPosition, wxDefaultSize, wxTE_READONLY );
    itemFlexGridSizer5->Add(itemTextCtrl16, 0, wxALIGN_CENTER_HORIZONTAL|wxALIGN_CENTER_VERTICAL, 5);

    wxButton* itemButton17 = new wxButton( itemDialog1, ID_K_Y_C, _("Change"), wxDefaultPosition, wxDefaultSize, 0 );
    itemFlexGridSizer5->Add(itemButton17, 0, wxALIGN_CENTER_HORIZONTAL|wxALIGN_CENTER_VERTICAL, 5);

    wxStaticText* itemStaticText18 = new wxStaticText( itemDialog1, wxID_STATIC, _("A:"), wxDefaultPosition, wxDefaultSize, 0 );
    itemFlexGridSizer5->Add(itemStaticText18, 0, wxALIGN_RIGHT|wxALIGN_CENTER_VERTICAL|wxRIGHT, 5);

    wxTextCtrl* itemTextCtrl19 = new wxTextCtrl( itemDialog1, ID_K_A_S, wxEmptyString, wxDefaultPosition, wxDefaultSize, wxTE_READONLY );
    itemFlexGridSizer5->Add(itemTextCtrl19, 0, wxALIGN_CENTER_HORIZONTAL|wxALIGN_CENTER_VERTICAL, 5);

    wxButton* itemButton20 = new wxButton( itemDialog1, ID_K_A_C, _("Change"), wxDefaultPosition, wxDefaultSize, 0 );
    itemFlexGridSizer5->Add(itemButton20, 0, wxALIGN_CENTER_HORIZONTAL|wxALIGN_CENTER_VERTICAL, 5);

    wxStaticText* itemStaticText21 = new wxStaticText( itemDialog1, wxID_STATIC, _("B:"), wxDefaultPosition, wxDefaultSize, 0 );
    itemFlexGridSizer5->Add(itemStaticText21, 0, wxALIGN_RIGHT|wxALIGN_CENTER_VERTICAL|wxRIGHT, 5);

    wxTextCtrl* itemTextCtrl22 = new wxTextCtrl( itemDialog1, ID_K_B_S, wxEmptyString, wxDefaultPosition, wxDefaultSize, wxTE_READONLY );
    itemFlexGridSizer5->Add(itemTextCtrl22, 0, wxALIGN_CENTER_HORIZONTAL|wxALIGN_CENTER_VERTICAL, 5);

    wxButton* itemButton23 = new wxButton( itemDialog1, ID_K_B_C, _("Change"), wxDefaultPosition, wxDefaultSize, 0 );
    itemFlexGridSizer5->Add(itemButton23, 0, wxALIGN_CENTER_HORIZONTAL|wxALIGN_CENTER_VERTICAL, 5);

    wxStaticText* itemStaticText24 = new wxStaticText( itemDialog1, wxID_STATIC, _("Start:"), wxDefaultPosition, wxDefaultSize, 0 );
    itemFlexGridSizer5->Add(itemStaticText24, 0, wxALIGN_RIGHT|wxALIGN_CENTER_VERTICAL|wxRIGHT, 5);

    wxTextCtrl* itemTextCtrl25 = new wxTextCtrl( itemDialog1, ID_K_START_S, wxEmptyString, wxDefaultPosition, wxDefaultSize, wxTE_READONLY );
    itemFlexGridSizer5->Add(itemTextCtrl25, 0, wxALIGN_CENTER_HORIZONTAL|wxALIGN_CENTER_VERTICAL, 5);

    wxButton* itemButton26 = new wxButton( itemDialog1, ID_K_START_C, _("Change"), wxDefaultPosition, wxDefaultSize, 0 );
    itemFlexGridSizer5->Add(itemButton26, 0, wxALIGN_CENTER_HORIZONTAL|wxALIGN_CENTER_VERTICAL, 5);

    wxStaticText* itemStaticText27 = new wxStaticText( itemDialog1, wxID_STATIC, _("Select:"), wxDefaultPosition, wxDefaultSize, 0 );
    itemFlexGridSizer5->Add(itemStaticText27, 0, wxALIGN_RIGHT|wxALIGN_CENTER_VERTICAL|wxRIGHT, 5);

    wxTextCtrl* itemTextCtrl28 = new wxTextCtrl( itemDialog1, ID_K_SELECT_S, wxEmptyString, wxDefaultPosition, wxDefaultSize, wxTE_READONLY );
    itemFlexGridSizer5->Add(itemTextCtrl28, 0, wxALIGN_CENTER_HORIZONTAL|wxALIGN_CENTER_VERTICAL, 5);

    wxButton* itemButton29 = new wxButton( itemDialog1, ID_K_SELECT_C, _("Change"), wxDefaultPosition, wxDefaultSize, 0 );
    itemFlexGridSizer5->Add(itemButton29, 0, wxALIGN_CENTER_HORIZONTAL|wxALIGN_CENTER_VERTICAL, 5);

    wxStaticText* itemStaticText30 = new wxStaticText( itemDialog1, wxID_STATIC, _("Up:"), wxDefaultPosition, wxDefaultSize, 0 );
    itemFlexGridSizer5->Add(itemStaticText30, 0, wxALIGN_RIGHT|wxALIGN_CENTER_VERTICAL|wxRIGHT, 5);

    wxTextCtrl* itemTextCtrl31 = new wxTextCtrl( itemDialog1, ID_K_UP_S, wxEmptyString, wxDefaultPosition, wxDefaultSize, wxTE_READONLY );
    itemFlexGridSizer5->Add(itemTextCtrl31, 0, wxALIGN_CENTER_HORIZONTAL|wxALIGN_CENTER_VERTICAL, 5);

    wxButton* itemButton32 = new wxButton( itemDialog1, ID_K_UP_C, _("Change"), wxDefaultPosition, wxDefaultSize, 0 );
    itemFlexGridSizer5->Add(itemButton32, 0, wxALIGN_CENTER_HORIZONTAL|wxALIGN_CENTER_VERTICAL, 5);

    wxStaticText* itemStaticText33 = new wxStaticText( itemDialog1, wxID_STATIC, _("Down:"), wxDefaultPosition, wxDefaultSize, 0 );
    itemFlexGridSizer5->Add(itemStaticText33, 0, wxALIGN_RIGHT|wxALIGN_CENTER_VERTICAL|wxRIGHT, 5);

    wxTextCtrl* itemTextCtrl34 = new wxTextCtrl( itemDialog1, ID_K_DOWN_S, wxEmptyString, wxDefaultPosition, wxDefaultSize, wxTE_READONLY );
    itemFlexGridSizer5->Add(itemTextCtrl34, 0, wxALIGN_CENTER_HORIZONTAL|wxALIGN_CENTER_VERTICAL, 5);

    wxButton* itemButton35 = new wxButton( itemDialog1, ID_K_DOWN_C, _("Change"), wxDefaultPosition, wxDefaultSize, 0 );
    itemFlexGridSizer5->Add(itemButton35, 0, wxALIGN_CENTER_HORIZONTAL|wxALIGN_CENTER_VERTICAL, 5);

    wxStaticText* itemStaticText36 = new wxStaticText( itemDialog1, wxID_STATIC, _("Left:"), wxDefaultPosition, wxDefaultSize, 0 );
    itemFlexGridSizer5->Add(itemStaticText36, 0, wxALIGN_RIGHT|wxALIGN_CENTER_VERTICAL|wxRIGHT, 5);

    wxTextCtrl* itemTextCtrl37 = new wxTextCtrl( itemDialog1, ID_K_LEFT_S, wxEmptyString, wxDefaultPosition, wxDefaultSize, wxTE_READONLY );
    itemFlexGridSizer5->Add(itemTextCtrl37, 0, wxALIGN_CENTER_HORIZONTAL|wxALIGN_CENTER_VERTICAL, 5);

    wxButton* itemButton38 = new wxButton( itemDialog1, ID_K_LEFT_C, _("Change"), wxDefaultPosition, wxDefaultSize, 0 );
    itemFlexGridSizer5->Add(itemButton38, 0, wxALIGN_CENTER_HORIZONTAL|wxALIGN_CENTER_VERTICAL, 5);

    wxStaticText* itemStaticText39 = new wxStaticText( itemDialog1, wxID_STATIC, _("Right:"), wxDefaultPosition, wxDefaultSize, 0 );
    itemFlexGridSizer5->Add(itemStaticText39, 0, wxALIGN_RIGHT|wxALIGN_CENTER_VERTICAL|wxRIGHT, 5);

    wxTextCtrl* itemTextCtrl40 = new wxTextCtrl( itemDialog1, ID_K_RIGHT_S, wxEmptyString, wxDefaultPosition, wxDefaultSize, wxTE_READONLY );
    itemFlexGridSizer5->Add(itemTextCtrl40, 0, wxALIGN_CENTER_HORIZONTAL|wxALIGN_CENTER_VERTICAL, 5);

    wxButton* itemButton41 = new wxButton( itemDialog1, ID_K_RIGHT_C, _("Change"), wxDefaultPosition, wxDefaultSize, 0 );
    itemFlexGridSizer5->Add(itemButton41, 0, wxALIGN_CENTER_HORIZONTAL|wxALIGN_CENTER_VERTICAL, 5);

    wxStaticText* itemStaticText42 = new wxStaticText( itemDialog1, wxID_STATIC, _("Close/open lid:"), wxDefaultPosition, wxDefaultSize, 0 );
    itemFlexGridSizer5->Add(itemStaticText42, 0, wxALIGN_RIGHT|wxALIGN_CENTER_VERTICAL|wxRIGHT, 5);

    wxTextCtrl* itemTextCtrl43 = new wxTextCtrl( itemDialog1, ID_K_LID_S, wxEmptyString, wxDefaultPosition, wxDefaultSize, wxTE_READONLY );
    itemFlexGridSizer5->Add(itemTextCtrl43, 0, wxALIGN_CENTER_HORIZONTAL|wxALIGN_CENTER_VERTICAL, 5);

    wxButton* itemButton44 = new wxButton( itemDialog1, ID_K_LID_C, _("Change"), wxDefaultPosition, wxDefaultSize, 0 );
    itemFlexGridSizer5->Add(itemButton44, 0, wxALIGN_CENTER_HORIZONTAL|wxALIGN_CENTER_VERTICAL, 5);

    wxStaticBox* itemStaticBoxSizer45Static = new wxStaticBox(itemDialog1, wxID_ANY, _("Joystick"));
    wxStaticBoxSizer* itemStaticBoxSizer45 = new wxStaticBoxSizer(itemStaticBoxSizer45Static, wxVERTICAL);
    itemBoxSizer3->Add(itemStaticBoxSizer45, 0, wxALIGN_TOP|wxALL, 5);

    wxArrayString itemChoice46Strings;
    wxChoice* itemChoice46 = new wxChoice( itemDialog1, ID_JOYSTICKIDX, wxDefaultPosition, wxSize(250, -1), itemChoice46Strings, 0 );
    itemStaticBoxSizer45->Add(itemChoice46, 0, wxALIGN_CENTER_HORIZONTAL|wxBOTTOM, 5);

    wxFlexGridSizer* itemFlexGridSizer47 = new wxFlexGridSizer(0, 3, 0, 0);
    itemStaticBoxSizer45->Add(itemFlexGridSizer47, 0, wxALIGN_CENTER_HORIZONTAL|wxLEFT|wxRIGHT|wxBOTTOM, 5);

    wxStaticText* itemStaticText48 = new wxStaticText( itemDialog1, wxID_STATIC, _("L:"), wxDefaultPosition, wxDefaultSize, 0 );
    itemFlexGridSizer47->Add(itemStaticText48, 0, wxALIGN_RIGHT|wxALIGN_CENTER_VERTICAL|wxRIGHT, 5);

    wxTextCtrl* itemTextCtrl49 = new wxTextCtrl( itemDialog1, ID_J_L_S, wxEmptyString, wxDefaultPosition, wxDefaultSize, wxTE_READONLY );
    itemFlexGridSizer47->Add(itemTextCtrl49, 0, wxALIGN_CENTER_HORIZONTAL|wxALIGN_CENTER_VERTICAL, 5);

    wxButton* itemButton50 = new wxButton( itemDialog1, ID_J_L_C, _("Change"), wxDefaultPosition, wxDefaultSize, 0 );
    itemFlexGridSizer47->Add(itemButton50, 0, wxALIGN_CENTER_HORIZONTAL|wxALIGN_CENTER_VERTICAL, 5);

    wxStaticText* itemStaticText51 = new wxStaticText( itemDialog1, wxID_STATIC, _("R:"), wxDefaultPosition, wxDefaultSize, 0 );
    itemFlexGridSizer47->Add(itemStaticText51, 0, wxALIGN_RIGHT|wxALIGN_CENTER_VERTICAL|wxRIGHT, 5);

    wxTextCtrl* itemTextCtrl52 = new wxTextCtrl( itemDialog1, ID_J_R_S, wxEmptyString, wxDefaultPosition, wxDefaultSize, wxTE_READONLY );
    itemFlexGridSizer47->Add(itemTextCtrl52, 0, wxALIGN_CENTER_HORIZONTAL|wxALIGN_CENTER_VERTICAL, 5);

    wxButton* itemButton53 = new wxButton( itemDialog1, ID_J_R_C, _("Change"), wxDefaultPosition, wxDefaultSize, 0 );
    itemFlexGridSizer47->Add(itemButton53, 0, wxALIGN_CENTER_HORIZONTAL|wxALIGN_CENTER_VERTICAL, 5);

    wxStaticText* itemStaticText54 = new wxStaticText( itemDialog1, wxID_STATIC, _("X:"), wxDefaultPosition, wxDefaultSize, 0 );
    itemFlexGridSizer47->Add(itemStaticText54, 0, wxALIGN_RIGHT|wxALIGN_CENTER_VERTICAL|wxRIGHT, 5);

    wxTextCtrl* itemTextCtrl55 = new wxTextCtrl( itemDialog1, ID_J_X_S, wxEmptyString, wxDefaultPosition, wxDefaultSize, wxTE_READONLY );
    itemFlexGridSizer47->Add(itemTextCtrl55, 0, wxALIGN_CENTER_HORIZONTAL|wxALIGN_CENTER_VERTICAL, 5);

    wxButton* itemButton56 = new wxButton( itemDialog1, ID_J_X_C, _("Change"), wxDefaultPosition, wxDefaultSize, 0 );
    itemFlexGridSizer47->Add(itemButton56, 0, wxALIGN_CENTER_HORIZONTAL|wxALIGN_CENTER_VERTICAL, 5);

    wxStaticText* itemStaticText57 = new wxStaticText( itemDialog1, wxID_STATIC, _("Y:"), wxDefaultPosition, wxDefaultSize, 0 );
    itemFlexGridSizer47->Add(itemStaticText57, 0, wxALIGN_RIGHT|wxALIGN_CENTER_VERTICAL|wxRIGHT, 5);

    wxTextCtrl* itemTextCtrl58 = new wxTextCtrl( itemDialog1, ID_J_Y_S, wxEmptyString, wxDefaultPosition, wxDefaultSize, wxTE_READONLY );
    itemFlexGridSizer47->Add(itemTextCtrl58, 0, wxALIGN_CENTER_HORIZONTAL|wxALIGN_CENTER_VERTICAL, 5);

    wxButton* itemButton59 = new wxButton( itemDialog1, ID_J_Y_C, _("Change"), wxDefaultPosition, wxDefaultSize, 0 );
    itemFlexGridSizer47->Add(itemButton59, 0, wxALIGN_CENTER_HORIZONTAL|wxALIGN_CENTER_VERTICAL, 5);

    wxStaticText* itemStaticText60 = new wxStaticText( itemDialog1, wxID_STATIC, _("A:"), wxDefaultPosition, wxDefaultSize, 0 );
    itemFlexGridSizer47->Add(itemStaticText60, 0, wxALIGN_RIGHT|wxALIGN_CENTER_VERTICAL|wxRIGHT, 5);

    wxTextCtrl* itemTextCtrl61 = new wxTextCtrl( itemDialog1, ID_J_A_S, wxEmptyString, wxDefaultPosition, wxDefaultSize, wxTE_READONLY );
    itemFlexGridSizer47->Add(itemTextCtrl61, 0, wxALIGN_CENTER_HORIZONTAL|wxALIGN_CENTER_VERTICAL, 5);

    wxButton* itemButton62 = new wxButton( itemDialog1, ID_J_A_C, _("Change"), wxDefaultPosition, wxDefaultSize, 0 );
    itemFlexGridSizer47->Add(itemButton62, 0, wxALIGN_CENTER_HORIZONTAL|wxALIGN_CENTER_VERTICAL, 5);

    wxStaticText* itemStaticText63 = new wxStaticText( itemDialog1, wxID_STATIC, _("B:"), wxDefaultPosition, wxDefaultSize, 0 );
    itemFlexGridSizer47->Add(itemStaticText63, 0, wxALIGN_RIGHT|wxALIGN_CENTER_VERTICAL|wxRIGHT, 5);

    wxTextCtrl* itemTextCtrl64 = new wxTextCtrl( itemDialog1, ID_J_B_S, wxEmptyString, wxDefaultPosition, wxDefaultSize, wxTE_READONLY );
    itemFlexGridSizer47->Add(itemTextCtrl64, 0, wxALIGN_CENTER_HORIZONTAL|wxALIGN_CENTER_VERTICAL, 5);

    wxButton* itemButton65 = new wxButton( itemDialog1, ID_J_B_C, _("Change"), wxDefaultPosition, wxDefaultSize, 0 );
    itemFlexGridSizer47->Add(itemButton65, 0, wxALIGN_CENTER_HORIZONTAL|wxALIGN_CENTER_VERTICAL, 5);

    wxStaticText* itemStaticText66 = new wxStaticText( itemDialog1, wxID_STATIC, _("Start:"), wxDefaultPosition, wxDefaultSize, 0 );
    itemFlexGridSizer47->Add(itemStaticText66, 0, wxALIGN_RIGHT|wxALIGN_CENTER_VERTICAL|wxRIGHT, 5);

    wxTextCtrl* itemTextCtrl67 = new wxTextCtrl( itemDialog1, ID_J_START_S, wxEmptyString, wxDefaultPosition, wxDefaultSize, wxTE_READONLY );
    itemFlexGridSizer47->Add(itemTextCtrl67, 0, wxALIGN_CENTER_HORIZONTAL|wxALIGN_CENTER_VERTICAL, 5);

    wxButton* itemButton68 = new wxButton( itemDialog1, ID_J_START_C, _("Change"), wxDefaultPosition, wxDefaultSize, 0 );
    itemFlexGridSizer47->Add(itemButton68, 0, wxALIGN_CENTER_HORIZONTAL|wxALIGN_CENTER_VERTICAL, 5);

    wxStaticText* itemStaticText69 = new wxStaticText( itemDialog1, wxID_STATIC, _("Select:"), wxDefaultPosition, wxDefaultSize, 0 );
    itemFlexGridSizer47->Add(itemStaticText69, 0, wxALIGN_RIGHT|wxALIGN_CENTER_VERTICAL|wxRIGHT, 5);

    wxTextCtrl* itemTextCtrl70 = new wxTextCtrl( itemDialog1, ID_J_SELECT_S, wxEmptyString, wxDefaultPosition, wxDefaultSize, wxTE_READONLY );
    itemFlexGridSizer47->Add(itemTextCtrl70, 0, wxALIGN_CENTER_HORIZONTAL|wxALIGN_CENTER_VERTICAL, 5);

    wxButton* itemButton71 = new wxButton( itemDialog1, ID_J_SELECT_C, _("Change"), wxDefaultPosition, wxDefaultSize, 0 );
    itemFlexGridSizer47->Add(itemButton71, 0, wxALIGN_CENTER_HORIZONTAL|wxALIGN_CENTER_VERTICAL, 5);

    wxStaticText* itemStaticText72 = new wxStaticText( itemDialog1, wxID_STATIC, _("Close/open lid:"), wxDefaultPosition, wxDefaultSize, 0 );
    itemFlexGridSizer47->Add(itemStaticText72, 0, wxALIGN_RIGHT|wxALIGN_CENTER_VERTICAL|wxRIGHT, 5);

    wxTextCtrl* itemTextCtrl73 = new wxTextCtrl( itemDialog1, ID_J_LID_S, wxEmptyString, wxDefaultPosition, wxDefaultSize, wxTE_READONLY );
    itemFlexGridSizer47->Add(itemTextCtrl73, 0, wxALIGN_CENTER_HORIZONTAL|wxALIGN_CENTER_VERTICAL, 5);

    wxButton* itemButton74 = new wxButton( itemDialog1, ID_J_LID_C, _("Change"), wxDefaultPosition, wxDefaultSize, 0 );
    itemFlexGridSizer47->Add(itemButton74, 0, wxALIGN_CENTER_HORIZONTAL|wxALIGN_CENTER_VERTICAL, 5);

    wxStaticBox* itemStaticBoxSizer75Static = new wxStaticBox(itemDialog1, wxID_ANY, _("D-Pad"));
    wxStaticBoxSizer* itemStaticBoxSizer75 = new wxStaticBoxSizer(itemStaticBoxSizer75Static, wxHORIZONTAL);
    itemStaticBoxSizer45->Add(itemStaticBoxSizer75, 0, wxALIGN_LEFT|wxALL, 5);

    wxRadioButton* itemRadioButton76 = new wxRadioButton( itemDialog1, ID_DPAD_OPT1, _("POV hat"), wxDefaultPosition, wxDefaultSize, 0 );
    itemRadioButton76->SetValue(false);
    itemStaticBoxSizer75->Add(itemRadioButton76, 0, wxALIGN_CENTER_VERTICAL|wxALL, 5);

    wxRadioButton* itemRadioButton77 = new wxRadioButton( itemDialog1, ID_DPAD_OPT2, _("X/Y axis"), wxDefaultPosition, wxDefaultSize, 0 );
    itemRadioButton77->SetValue(false);
    itemStaticBoxSizer75->Add(itemRadioButton77, 0, wxALIGN_CENTER_VERTICAL|wxALL, 5);

    wxRadioButton* itemRadioButton78 = new wxRadioButton( itemDialog1, ID_DPAD_OPT3, _("Both"), wxDefaultPosition, wxDefaultSize, 0 );
    itemRadioButton78->SetValue(false);
    itemStaticBoxSizer75->Add(itemRadioButton78, 0, wxALIGN_CENTER_VERTICAL|wxALL, 5);

    wxBoxSizer* itemBoxSizer79 = new wxBoxSizer(wxHORIZONTAL);
    itemBoxSizer2->Add(itemBoxSizer79, 0, wxALIGN_RIGHT|wxLEFT|wxRIGHT|wxBOTTOM, 5);

    wxButton* itemButton80 = new wxButton( itemDialog1, wxID_DEFAULT, _("Default"), wxDefaultPosition, wxDefaultSize, 0 );
    itemBoxSizer79->Add(itemButton80, 0, wxALIGN_CENTER_VERTICAL|wxLEFT|wxTOP|wxBOTTOM, 5);

    itemBoxSizer79->Add(5, 5, 0, wxALIGN_CENTER_VERTICAL|wxALL, 5);

    wxButton* itemButton82 = new wxButton( itemDialog1, wxID_OK, _("OK"), wxDefaultPosition, wxDefaultSize, 0 );
    itemButton82->SetDefault();
    itemBoxSizer79->Add(itemButton82, 0, wxALIGN_CENTER_VERTICAL|wxTOP|wxBOTTOM, 5);

    wxButton* itemButton83 = new wxButton( itemDialog1, wxID_CANCEL, _("Cancel"), wxDefaultPosition, wxDefaultSize, 0 );
    itemBoxSizer79->Add(itemButton83, 0, wxALIGN_CENTER_VERTICAL|wxTOP|wxBOTTOM, 5);

    wxButton* itemButton84 = new wxButton( itemDialog1, wxID_APPLY, _("&Apply"), wxDefaultPosition, wxDefaultSize, 0 );
    itemButton84->Enable(false);
    itemBoxSizer79->Add(itemButton84, 0, wxALIGN_CENTER_VERTICAL|wxRIGHT|wxTOP|wxBOTTOM, 5);

////@end wxControlsConfigDialog content construction
}


/*
 * Should we show tooltips?
 */

bool wxControlsConfigDialog::ShowToolTips()
{
    return true;
}

/*
 * Get bitmap resources
 */

wxBitmap wxControlsConfigDialog::GetBitmapResource( const wxString& name )
{
    // Bitmap retrieval
////@begin wxControlsConfigDialog bitmap retrieval
    wxUnusedVar(name);
    return wxNullBitmap;
////@end wxControlsConfigDialog bitmap retrieval
}

/*
 * Get icon resources
 */

wxIcon wxControlsConfigDialog::GetIconResource( const wxString& name )
{
    // Icon retrieval
////@begin wxControlsConfigDialog icon retrieval
    wxUnusedVar(name);
    return wxNullIcon;
////@end wxControlsConfigDialog icon retrieval
}


/*
 * wxEVT_COMMAND_BUTTON_CLICKED event handler for ID_K_L_C
 */

void wxControlsConfigDialog::OnChangeKeyboardMapping( wxCommandEvent& event )
{
////@begin wxEVT_COMMAND_BUTTON_CLICKED event handler for ID_K_L_C in wxControlsConfigDialog.
    // Before editing this code, remove the block markers.
    event.Skip();
////@end wxEVT_COMMAND_BUTTON_CLICKED event handler for ID_K_L_C in wxControlsConfigDialog. 
}


/*
 * wxEVT_COMMAND_CHOICE_SELECTED event handler for ID_JOYSTICKIDX
 */

void wxControlsConfigDialog::OnJoystickidxSelected( wxCommandEvent& event )
{
////@begin wxEVT_COMMAND_CHOICE_SELECTED event handler for ID_JOYSTICKIDX in wxControlsConfigDialog.
    // Before editing this code, remove the block markers.
    event.Skip();
////@end wxEVT_COMMAND_CHOICE_SELECTED event handler for ID_JOYSTICKIDX in wxControlsConfigDialog. 
}


/*
 * wxEVT_COMMAND_BUTTON_CLICKED event handler for ID_J_L_C
 */

void wxControlsConfigDialog::OnChangeJoystickMapping( wxCommandEvent& event )
{
////@begin wxEVT_COMMAND_BUTTON_CLICKED event handler for ID_J_L_C in wxControlsConfigDialog.
    // Before editing this code, remove the block markers.
    event.Skip();
////@end wxEVT_COMMAND_BUTTON_CLICKED event handler for ID_J_L_C in wxControlsConfigDialog. 
}


/*
 * wxEVT_COMMAND_RADIOBUTTON_SELECTED event handler for ID_DPAD_OPT1
 */

void wxControlsConfigDialog::OnChangeDPadMode( wxCommandEvent& event )
{
////@begin wxEVT_COMMAND_RADIOBUTTON_SELECTED event handler for ID_DPAD_OPT1 in wxControlsConfigDialog.
    // Before editing this code, remove the block markers.
    event.Skip();
////@end wxEVT_COMMAND_RADIOBUTTON_SELECTED event handler for ID_DPAD_OPT1 in wxControlsConfigDialog. 
}


/*
 * wxEVT_COMMAND_BUTTON_CLICKED event handler for wxID_DEFAULT
 */

void wxControlsConfigDialog::OnDefaultClick( wxCommandEvent& event )
{
////@begin wxEVT_COMMAND_BUTTON_CLICKED event handler for wxID_DEFAULT in wxControlsConfigDialog.
    // Before editing this code, remove the block markers.
    event.Skip();
////@end wxEVT_COMMAND_BUTTON_CLICKED event handler for wxID_DEFAULT in wxControlsConfigDialog. 
}


/*
 * wxEVT_COMMAND_BUTTON_CLICKED event handler for wxID_OK
 */

void wxControlsConfigDialog::OnOkClick( wxCommandEvent& event )
{
////@begin wxEVT_COMMAND_BUTTON_CLICKED event handler for wxID_OK in wxControlsConfigDialog.
    // Before editing this code, remove the block markers.
    event.Skip();
////@end wxEVT_COMMAND_BUTTON_CLICKED event handler for wxID_OK in wxControlsConfigDialog. 
}


/*
 * wxEVT_COMMAND_BUTTON_CLICKED event handler for wxID_CANCEL
 */

void wxControlsConfigDialog::OnCancelClick( wxCommandEvent& event )
{
	Destroy();
}


/*
 * wxEVT_COMMAND_BUTTON_CLICKED event handler for wxID_APPLY
 */

void wxControlsConfigDialog::OnApplyClick( wxCommandEvent& event )
{
////@begin wxEVT_COMMAND_BUTTON_CLICKED event handler for wxID_APPLY in wxControlsConfigDialog.
    // Before editing this code, remove the block markers.
    event.Skip();
////@end wxEVT_COMMAND_BUTTON_CLICKED event handler for wxID_APPLY in wxControlsConfigDialog. 
}

