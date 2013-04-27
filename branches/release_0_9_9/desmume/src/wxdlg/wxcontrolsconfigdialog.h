/////////////////////////////////////////////////////////////////////////////
// Name:        wxcontrolsconfigdialog.h
// Purpose:     
// Author:      
// Modified by: 
// Created:     09/07/2010 15:27:00
// RCS-ID:      
// Copyright:   
// Licence:     
/////////////////////////////////////////////////////////////////////////////

#ifndef _WXCONTROLSCONFIGDIALOG_H_
#define _WXCONTROLSCONFIGDIALOG_H_


/*!
 * Includes
 */

////@begin includes
////@end includes

/*!
 * Forward declarations
 */

////@begin forward declarations
////@end forward declarations

/*!
 * Control identifiers
 */

////@begin control identifiers
#define ID_WXCONTROLSCONFIGDIALOG 10010
#define ID_K_L_S 10011
#define ID_K_L_C 10012
#define ID_K_R_S 10013
#define ID_K_R_C 10014
#define ID_K_X_S 10015
#define ID_K_X_C 10016
#define ID_K_Y_S 10017
#define ID_K_Y_C 10018
#define ID_K_A_S 10019
#define ID_K_A_C 10020
#define ID_K_B_S 10021
#define ID_K_B_C 10022
#define ID_K_START_S 10023
#define ID_K_START_C 10024
#define ID_K_SELECT_S 10025
#define ID_K_SELECT_C 10026
#define ID_K_UP_S 10027
#define ID_K_UP_C 10028
#define ID_K_DOWN_S 10029
#define ID_K_DOWN_C 10030
#define ID_K_LEFT_S 10031
#define ID_K_LEFT_C 10032
#define ID_K_RIGHT_S 10033
#define ID_K_RIGHT_C 10034
#define ID_K_LID_S 10035
#define ID_K_LID_C 10036
#define ID_JOYSTICKIDX 10058
#define ID_J_L_S 10037
#define ID_J_L_C 10038
#define ID_J_R_S 10039
#define ID_J_R_C 10040
#define ID_J_X_S 10041
#define ID_J_X_C 10042
#define ID_J_Y_S 10043
#define ID_J_Y_C 10044
#define ID_J_A_S 10045
#define ID_J_A_C 10046
#define ID_J_B_S 10047
#define ID_J_B_C 10048
#define ID_J_START_S 10049
#define ID_J_START_C 10050
#define ID_J_SELECT_S 10051
#define ID_J_SELECT_C 10052
#define ID_J_LID_S 10053
#define ID_J_LID_C 10054
#define ID_DPAD_OPT1 10055
#define ID_DPAD_OPT2 10056
#define ID_DPAD_OPT3 10057
#define SYMBOL_WXCONTROLSCONFIGDIALOG_STYLE wxCAPTION|wxSYSTEM_MENU|wxCLOSE_BOX|wxTAB_TRAVERSAL
#define SYMBOL_WXCONTROLSCONFIGDIALOG_TITLE _("Controls configuration")
#define SYMBOL_WXCONTROLSCONFIGDIALOG_IDNAME ID_WXCONTROLSCONFIGDIALOG
#define SYMBOL_WXCONTROLSCONFIGDIALOG_SIZE wxSize(400, 300)
#define SYMBOL_WXCONTROLSCONFIGDIALOG_POSITION wxDefaultPosition
////@end control identifiers


/*!
 * wxControlsConfigDialog class declaration
 */

class wxControlsConfigDialog: public wxDialog
{    
    DECLARE_DYNAMIC_CLASS( wxControlsConfigDialog )
    DECLARE_EVENT_TABLE()

public:
    /// Constructors
    wxControlsConfigDialog();
    wxControlsConfigDialog( wxWindow* parent, wxWindowID id = SYMBOL_WXCONTROLSCONFIGDIALOG_IDNAME, const wxString& caption = SYMBOL_WXCONTROLSCONFIGDIALOG_TITLE, const wxPoint& pos = SYMBOL_WXCONTROLSCONFIGDIALOG_POSITION, const wxSize& size = SYMBOL_WXCONTROLSCONFIGDIALOG_SIZE, long style = SYMBOL_WXCONTROLSCONFIGDIALOG_STYLE );

    /// Creation
    bool Create( wxWindow* parent, wxWindowID id = SYMBOL_WXCONTROLSCONFIGDIALOG_IDNAME, const wxString& caption = SYMBOL_WXCONTROLSCONFIGDIALOG_TITLE, const wxPoint& pos = SYMBOL_WXCONTROLSCONFIGDIALOG_POSITION, const wxSize& size = SYMBOL_WXCONTROLSCONFIGDIALOG_SIZE, long style = SYMBOL_WXCONTROLSCONFIGDIALOG_STYLE );

    /// Destructor
    ~wxControlsConfigDialog();

    /// Initialises member variables
    void Init();

    /// Creates the controls and sizers
    void CreateControls();

////@begin wxControlsConfigDialog event handler declarations

    /// wxEVT_COMMAND_BUTTON_CLICKED event handler for ID_K_L_C
    virtual void OnChangeKeyboardMapping( wxCommandEvent& event );

    /// wxEVT_COMMAND_CHOICE_SELECTED event handler for ID_JOYSTICKIDX
    virtual void OnJoystickidxSelected( wxCommandEvent& event );

    /// wxEVT_COMMAND_BUTTON_CLICKED event handler for ID_J_L_C
    virtual void OnChangeJoystickMapping( wxCommandEvent& event );

    /// wxEVT_COMMAND_RADIOBUTTON_SELECTED event handler for ID_DPAD_OPT1
    virtual void OnChangeDPadMode( wxCommandEvent& event );

    /// wxEVT_COMMAND_BUTTON_CLICKED event handler for wxID_DEFAULT
    virtual void OnDefaultClick( wxCommandEvent& event );

    /// wxEVT_COMMAND_BUTTON_CLICKED event handler for wxID_OK
    virtual void OnOkClick( wxCommandEvent& event );

    /// wxEVT_COMMAND_BUTTON_CLICKED event handler for wxID_CANCEL
    virtual void OnCancelClick( wxCommandEvent& event );

    /// wxEVT_COMMAND_BUTTON_CLICKED event handler for wxID_APPLY
    virtual void OnApplyClick( wxCommandEvent& event );

////@end wxControlsConfigDialog event handler declarations

////@begin wxControlsConfigDialog member function declarations

    /// Retrieves bitmap resources
    wxBitmap GetBitmapResource( const wxString& name );

    /// Retrieves icon resources
    wxIcon GetIconResource( const wxString& name );
////@end wxControlsConfigDialog member function declarations

    /// Should we show tooltips?
    static bool ShowToolTips();

////@begin wxControlsConfigDialog member variables
////@end wxControlsConfigDialog member variables
};

#endif
    // _WXCONTROLSCONFIGDIALOG_H_
