
#include "NDSSystem.h"
#include <wx/wxprec.h>
#include "movie.h"
#include <wx/docview.h>
#include <wx/config.h>
static int nds_screen_rotation_angle;
static wxFileConfig *desmumeConfig;
enum
{
	wPause = 1,
	wReset,
	wFrameCounter,
	wFPS,
	wDisplayInput,
	wDisplayGraphicalInput,
	wDisplayLagCounter,
	wDisplayMicrophone,
	wSetHUDFont,
	wMainGPU,
	wMainBG0,
	wMainBG1,
	wMainBG2,
	wMainBG3,
	wSubGPU,
	wSubBG0,
	wSubBG1,
	wSubBG2,
	wSubBG3,
	wWebsite,
	wForums,
	wSubmitABugReport,
	w3dView,
	wSaveStateAs,
	wLoadStateFrom,
	wCloseRom,
	wImportBackupMemory,
	wExportBackupMemory,
	wPlayMovie,
	wRecordMovie,
	wStopMovie,
	wSaveScreenshotAs,
	wQuickScreenshot,
	wLuaWindow,
	wConfigureControls,
	wRot0,
	wRot90,
	wRot180,
	wRot270,
	/* stupid enums: these two should be the at the end */
	wLoadState01,
	wSaveState01 = wLoadState01+20
};

class Desmume: public wxApp
{
public:
	virtual bool OnInit();
};

class DesmumeFrame: public wxFrame
{
public:
	DesmumeFrame(const wxString& title);
	~DesmumeFrame() {delete history;}
	void OnQuit(wxCommandEvent& WXUNUSED(event));
	void OnAbout(wxCommandEvent& WXUNUSED(event));
	void applyInput();
	//TODO integrate paths system?
	void LoadRom(wxCommandEvent& event);
	//----------------------------------------------------------------------------
	//   Touchscreen
	//----------------------------------------------------------------------------

	// De-transform coordinates.
	// Returns true if the coordinates are within the touchscreen.
	bool DetransformTouchCoords(int& X, int& Y);
	void OnTouchEvent(wxMouseEvent& evt);
	//----------------------------------------------------------------------------
	//   Video
	//----------------------------------------------------------------------------
	void gpu_screen_to_rgb(u8 *rgb1, u8 *rgb2);
	//TODO should integrate filter system
	void onPaint(wxPaintEvent &event);
	void onIdle(wxIdleEvent &event);
	void pause(wxCommandEvent& event);
	void reset(wxCommandEvent& event){NDS_Reset();}
	void frameCounter(wxCommandEvent& event);
	void FPS(wxCommandEvent& event);
	void displayInput(wxCommandEvent& event);
	void displayGraphicalInput(wxCommandEvent& event);
	void displayLagCounter(wxCommandEvent& event);
	void displayMicrophone(wxCommandEvent& event);
	void setHUDFont(wxCommandEvent &event);

	void mainG(int n);
	void subG(int n);

	void mainGPU(wxCommandEvent& event){CommonSettings.showGpu.main^=true;}
	void mainBG0(wxCommandEvent& event){mainG(0);}
	void mainBG1(wxCommandEvent& event){mainG(1);}
	void mainBG2(wxCommandEvent& event){mainG(2);}
	void mainBG3(wxCommandEvent& event){mainG(3);}

	void subGPU(wxCommandEvent& event){CommonSettings.showGpu.sub^=true;}
	void subBG0(wxCommandEvent& event){subG(0);}
	void subBG1(wxCommandEvent& event){subG(1);}
	void subBG2(wxCommandEvent& event){subG(2);}
	void subBG3(wxCommandEvent& event){subG(3);}

	void website(wxCommandEvent& event) {wxLaunchDefaultBrowser(_T("http://desmume.org/"));}
	void forums(wxCommandEvent& event) {wxLaunchDefaultBrowser(_T("http://forums.desmume.org/index.php"));}
	void submitABugReport(wxCommandEvent& event) {wxLaunchDefaultBrowser(_T("http://sourceforge.net/tracker/?func=add&group_id=164579&atid=832291"));}

	void _3dView(wxCommandEvent& event);
	void saveStateAs(wxCommandEvent& event);
	void loadStateFrom(wxCommandEvent& event);

	void closeRom(wxCommandEvent& event);
	void importBackupMemory(wxCommandEvent& event);
	void exportBackupMemory(wxCommandEvent& event);
	void saveScreenshotAs(wxCommandEvent& event);
	void quickScreenshot(wxCommandEvent& event);
	//TODO
	void playMovie(wxCommandEvent& event) {}
	void recordMovie(wxCommandEvent& event) {}
	void stopMovie(wxCommandEvent& event) {FCEUI_StopMovie();}

	void OnOpenLuaWindow(wxCommandEvent& WXUNUSED (event));
	void OnOpenControllerConfiguration(wxCommandEvent& WXUNUSED (event));
	wxMenu* MakeStatesSubMenu( int baseid ) const
	{
		wxMenu* mnuSubstates = new wxMenu();

		for (int i = 0; i < 10; i++)
		{
			mnuSubstates->Append( baseid+i, wxString::Format(_T("Slot %d"), i) );
		}
		return mnuSubstates;
	}
	void Menu_SaveStates(wxCommandEvent &event);
	void Menu_LoadStates(wxCommandEvent &event);
	void NDSInitialize();
	void OnRotation(wxCommandEvent &event);
	void ChangeRotation(int rot, bool skip);
	void onResize(wxSizeEvent &event);
	bool LoadSettings();
	bool SaveSettings();
	void OnClose(wxCloseEvent &event);
	void OnOpenRecent(wxCommandEvent &event);

private:
	void loadfileMenu(wxMenu *fileMenu);
	void loadmenuBar(wxMenuBar *menuBar);
	void loadhelpMenu(wxMenu *helpMenu);
	void loadconfigMenu(wxMenu *configMenu);
	void loadtoolsMenu(wxMenu *toolsMenu);
	void loadlayersMenu(wxMenu *layersMenu);
	void loadviewMenu(wxMenu *viewMenu);
	void loadrotateMenu(wxMenu *rotateMenu);
	void loademulationMenu(wxMenu *emulationMenu);
	struct NDS_fw_config_data fw_config;
	wxFileHistory* history;
#ifdef GDB_STUB
	gdbstub_handle_t arm9_gdb_stub;
	gdbstub_handle_t arm7_gdb_stub;
	struct armcpu_memory_iface *arm9_memio;
	struct armcpu_memory_iface *arm7_memio;
	struct armcpu_ctrl_iface *arm9_ctrl_iface;
	struct armcpu_ctrl_iface *arm7_ctrl_iface;
#endif
	DECLARE_EVENT_TABLE()
};