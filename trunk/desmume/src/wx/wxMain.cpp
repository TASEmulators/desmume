#ifdef WIN32
#pragma comment(lib,"wxmsw28_core.lib")
#pragma comment(lib,"wxbase28.lib")
#else
#define lstrlen(a) strlen((a))
#endif

#undef WIN32
#include "NDSSystem.h"
#include "GPU_osd.h"
#include "wx/wxprec.h"
#include "gfx3d.h"
#include "version.h"
#include "addons.h"
#include "saves.h"
#include "movie.h"

#include "wx/stdpaths.h"

#include "LuaWindow.h"
#include "PadSimple/GUI/ConfigDlg.h"
#include "PadSimple/pluginspecs_pad.h"

#ifndef WX_PRECOMP
#include "wx/wx.h"
#endif

std::string executableDirectory;

class Desmume: public wxApp
{
public:
	virtual bool OnInit();
};

class DesmumeFrame: public wxFrame
{
public:
	DesmumeFrame(const wxString& title);

	void OnQuit(wxCommandEvent& WXUNUSED(event)){Close(true);}
	void OnAbout(wxCommandEvent& WXUNUSED(event))
	{
		wxMessageBox(wxString::Format
			(
			"Desmume on %s",
			wxGetOsDescription()
			),
			"About Desmume",
			wxOK | wxICON_INFORMATION,
			this);
	}

	void applyInput(){

		bool up,down,left,right,x,y,a,b,l,r,start,select;

		up = down = left = right = x = y = a = b = l = r = start = select = false;

		SPADStatus s;
		memset(&s,0,sizeof(s));
		PAD_GetStatus(0, &s);

		if(s.button & PAD_BUTTON_LEFT)
			left = true;
		if(s.button & PAD_BUTTON_RIGHT)
			right = true;
		if(s.button & PAD_BUTTON_UP)
			up = true;
		if(s.button & PAD_BUTTON_DOWN)
			down = true;
		if(s.button & PAD_BUTTON_A)
			a = true;
		if(s.button & PAD_BUTTON_B)
			b = true;
		if(s.button & PAD_BUTTON_X)
			x = true;
		if(s.button & PAD_BUTTON_Y)
			y = true;
		if(s.button & PAD_TRIGGER_L)
			l = true;
		if(s.button & PAD_TRIGGER_R)
			r = true;
		if(s.button & PAD_BUTTON_START)
			start = true;

		u16 pad1 = (0 |
			((a ? 0 : 0x80) >> 7) |
			((b ? 0 : 0x80) >> 6) |
			((select? 0 : 0x80) >> 5) |
			((start ? 0 : 0x80) >> 4) |
			((right ? 0 : 0x80) >> 3) |
			((left ? 0 : 0x80) >> 2) |
			((up ? 0 : 0x80) >> 1) |
			((down ? 0 : 0x80) ) |
			((r ? 0 : 0x80) << 1) |
			((l ? 0 : 0x80) << 2)) ;

		((u16 *)MMU.ARM9_REG)[0x130>>1] = (u16)pad1;
		((u16 *)MMU.ARM7_REG)[0x130>>1] = (u16)pad1;

		bool debug = false;
		bool lidClosed = false;

		u16 padExt = (((u16 *)MMU.ARM7_REG)[0x136>>1] & 0x0070) |
			((x ? 0 : 0x80) >> 7) |
			((y ? 0 : 0x80) >> 6) |
			((debug ? 0 : 0x80) >> 4) |
			((lidClosed) << 7) |
			0x0034;

		((u16 *)MMU.ARM7_REG)[0x136>>1] = (u16)padExt;
	}

	//TODO integrate paths system?
	void LoadRom(wxCommandEvent& event){
		wxFileDialog dialog(this,(wxChar *)"Load Rom",wxGetHomeDir(),(wxChar *)"",(wxChar *)"*.nds",wxFD_OPEN, wxDefaultPosition, wxDefaultSize);
		if(dialog.ShowModal() == wxID_OK) {
			NDS_Init ();
			execute = true;
			NDS_LoadROM(dialog.GetPath(), dialog.GetPath());
		}
	}

	//TODO should integrate filter system
	void onPaint(wxPaintEvent &event)
	{
		wxPaintDC dc(this);

		int width = 256;
		int height = 384;

		u16 gpu_pixel;
		u32 offset;
		u8 rgb[256*384*3];
		for (int i = 0; i < 256; i++) {
			for (int j = 0; j < 384; j++) {
				gpu_pixel = *((u16 *) & GPU_screen[(i + j * 256) << 1]);
				offset = i * 3 + j * 3 * 256;
				*(rgb + offset + 0) = ((gpu_pixel >> 0) & 0x1f) << 3;
				*(rgb + offset + 1) = ((gpu_pixel >> 5) & 0x1f) << 3;
				*(rgb + offset + 2) = ((gpu_pixel >> 10) & 0x1f) << 3;
			}
		}

		wxBitmap m_bitmap(wxImage(width, height, rgb, true));
		dc.DrawBitmap(m_bitmap, 0, 0, true);
	}

	void onIdle(wxIdleEvent &event){
		Refresh(false);
		event.RequestMore();
		applyInput();
		if(execute) 
			NDS_exec<false>();
		osd->update();
		DrawHUD();
		osd->clear();
		// wxMicroSleep(16.7*1000);
	}

	void pause(wxCommandEvent& event){
		execute ? execute=false : execute=true;
	}
	void reset(wxCommandEvent& event){NDS_Reset();}

	void frameCounter(wxCommandEvent& event){
		CommonSettings.hud.FrameCounterDisplay ^= true;
		osd->clear();
	}

	void FPS(wxCommandEvent& event){
		CommonSettings.hud.FpsDisplay ^= true;
		osd->clear();
	}
	void displayInput(wxCommandEvent& event){
		CommonSettings.hud.ShowInputDisplay ^= true;
		osd->clear();
	}
	void displayGraphicalInput(wxCommandEvent& event){
		CommonSettings.hud.ShowGraphicalInputDisplay ^= true;
		osd->clear();
	}
	void displayLagCounter(wxCommandEvent& event){
		CommonSettings.hud.ShowLagFrameCounter ^= true;
		osd->clear();
	}
	void displayMicrophone(wxCommandEvent& event){
		CommonSettings.hud.ShowMicrophone ^= true;
		osd->clear();
	}

	void mainG(int n) {
		if(MainScreen.gpu->dispBG[n])
			GPU_remove(MainScreen.gpu, n);
		else
			GPU_addBack(MainScreen.gpu, n);	
	}
	void subG(int n) {
		if(SubScreen.gpu->dispBG[n])
			GPU_remove(SubScreen.gpu, n);
		else
			GPU_addBack(SubScreen.gpu, n);	
	}
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

	void website(wxCommandEvent& event) {wxLaunchDefaultBrowser((wxChar *)"http://desmume.org/");}
	void forums(wxCommandEvent& event) {wxLaunchDefaultBrowser((wxChar *)"http://forums.desmume.org/index.php");}
	void submitABugReport(wxCommandEvent& event) {wxLaunchDefaultBrowser((wxChar *)"http://sourceforge.net/tracker/?func=add&group_id=164579&atid=832291");}

	void _3dView(wxCommandEvent& event) {
		driver->VIEW3D_Init();
		driver->view3d->Launch();
	}

	void saveStateAs(wxCommandEvent& event) {
		wxFileDialog dialog(this,(wxChar *)"Save State As",wxGetHomeDir(),(wxChar *)"",(wxChar *)"*.dst",wxFD_SAVE, wxDefaultPosition, wxDefaultSize);
		if(dialog.ShowModal() == wxID_OK)
			savestate_save (dialog.GetPath());
	}
	void loadStateFrom(wxCommandEvent& event) {
		wxFileDialog dialog(this,(wxChar *)"Load State From",wxGetHomeDir(),(wxChar *)"",(wxChar *)"*.dst",wxFD_OPEN, wxDefaultPosition, wxDefaultSize);
		if(dialog.ShowModal() == wxID_OK)
			savestate_load (dialog.GetPath());
	}

	void closeRom(wxCommandEvent& event) {
		NDS_FreeROM();
		execute = false;
		Hud.resetTransient();
		NDS_Reset();
	}

	void importBackupMemory(wxCommandEvent& event) {
		wxFileDialog dialog(this,(wxChar *)"Import Backup Memory",wxGetHomeDir(),(wxChar *)"",(wxChar *)"*.duc, *.sav",wxFD_OPEN, wxDefaultPosition, wxDefaultSize);
		if(dialog.ShowModal() == wxID_OK)
			if (!NDS_ImportSave(dialog.GetPath()))
				wxMessageBox(wxString::Format("Save was not successfully imported"),(wxChar *)"Error",wxOK | wxICON_ERROR,this);
	}

	void exportBackupMemory(wxCommandEvent& event) {
		wxFileDialog dialog(this,(wxChar *)"Export Backup Memory",wxGetHomeDir(),(wxChar *)"",(wxChar *)"*.duc, *.sav",wxFD_SAVE, wxDefaultPosition, wxDefaultSize);
		if(dialog.ShowModal() == wxID_OK)
			if (!NDS_ExportSave(dialog.GetPath()))
				wxMessageBox(wxString::Format("Save was not successfully exported"),"Error",wxOK | wxICON_ERROR,this);
	}
	void saveScreenshotAs(wxCommandEvent& event) {
		wxFileDialog dialog(this,(wxChar *)"Save Screenshot As",wxGetHomeDir(),(wxChar *)"",(wxChar *)"*.png",wxFD_SAVE, wxDefaultPosition, wxDefaultSize);
		if(dialog.ShowModal() == wxID_OK)
			NDS_WritePNG(dialog.GetPath());
	}
	void quickScreenshot(wxCommandEvent& event) {
			NDS_WritePNG(wxStandardPaths::Get().GetExecutablePath());//TODO GetExecutablePath is wrong
	}

	//TODO
	void playMovie(wxCommandEvent& event) {}
	void recordMovie(wxCommandEvent& event) {}
	void stopMovie(wxCommandEvent& event) {FCEUI_StopMovie();}

	void OnOpenLuaWindow(wxCommandEvent& WXUNUSED (event))
	{
		new wxLuaWindow(this, wxDefaultPosition, wxSize(600, 390));
	}

	void OnOpenControllerConfiguration(wxCommandEvent& WXUNUSED (event))
	{
		new PADConfigDialogSimple(this);
	}

	wxMenu* MakeStatesSubMenu( int baseid ) const
	{
		wxMenu* mnuSubstates = new wxMenu();

		for (int i = 0; i < 10; i++)
		{
			mnuSubstates->Append( baseid+i+1, wxString::Format((wxChar *)"Slot %d", i) );
		}
		return mnuSubstates;
	}
	void Menu_SaveStates(wxCommandEvent &event);
	void Menu_LoadStates(wxCommandEvent &event);

private:
	DECLARE_EVENT_TABLE()
};

enum
{
	wExit = wxID_EXIT,
	wAbout = wxID_ABOUT,
	wRom,
	wPause,
	wReset,
	wFrameCounter,
	wFPS,
	wDisplayInput,
	wDisplayGraphicalInput,
	wDisplayLagCounter,
	wDisplayMicrophone,
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
	wLoadState01,
	wSaveState01 = wLoadState01+20,
	wConfigureControls
};

void DesmumeFrame::Menu_SaveStates(wxCommandEvent &event){savestate_slot(event.GetId() - wSaveState01 - 1);}
void DesmumeFrame::Menu_LoadStates(wxCommandEvent &event){loadstate_slot(event.GetId() - wLoadState01 - 1);}

BEGIN_EVENT_TABLE(DesmumeFrame, wxFrame)
EVT_PAINT(DesmumeFrame::onPaint)
EVT_IDLE(DesmumeFrame::onIdle)
EVT_MENU(wExit, DesmumeFrame::OnQuit)
EVT_MENU(wRom, DesmumeFrame::LoadRom)
EVT_MENU(wAbout,DesmumeFrame::OnAbout)
EVT_MENU(wPause,DesmumeFrame::pause)
EVT_MENU(wReset,DesmumeFrame::reset)
EVT_MENU(wFrameCounter,DesmumeFrame::frameCounter)
EVT_MENU(wFPS,DesmumeFrame::FPS)
EVT_MENU(wDisplayInput,DesmumeFrame::displayInput)
EVT_MENU(wDisplayGraphicalInput,DesmumeFrame::displayGraphicalInput)
EVT_MENU(wDisplayLagCounter,DesmumeFrame::displayLagCounter)
EVT_MENU(wDisplayMicrophone,DesmumeFrame::displayMicrophone)

EVT_MENU(wMainGPU,DesmumeFrame::mainGPU)
EVT_MENU(wMainBG0,DesmumeFrame::mainBG0)
EVT_MENU(wMainBG1,DesmumeFrame::mainBG1)
EVT_MENU(wMainBG2,DesmumeFrame::mainBG2)
EVT_MENU(wMainBG3,DesmumeFrame::mainBG3)
EVT_MENU(wSubGPU,DesmumeFrame::subGPU)
EVT_MENU(wSubBG0,DesmumeFrame::subBG0)
EVT_MENU(wSubBG1,DesmumeFrame::subBG1)
EVT_MENU(wSubBG2,DesmumeFrame::subBG2)
EVT_MENU(wSubBG3,DesmumeFrame::subBG3)

EVT_MENU(wWebsite,DesmumeFrame::website)
EVT_MENU(wForums,DesmumeFrame::forums)
EVT_MENU(wSubmitABugReport,DesmumeFrame::submitABugReport)

EVT_MENU(wSaveStateAs,DesmumeFrame::saveStateAs)
EVT_MENU(wLoadStateFrom,DesmumeFrame::loadStateFrom)

EVT_MENU(wCloseRom,DesmumeFrame::closeRom)
EVT_MENU(wImportBackupMemory,DesmumeFrame::importBackupMemory)
EVT_MENU(wExportBackupMemory,DesmumeFrame::exportBackupMemory)

EVT_MENU(wSaveScreenshotAs,DesmumeFrame::saveScreenshotAs)
EVT_MENU(wQuickScreenshot,DesmumeFrame::quickScreenshot)

EVT_MENU(wPlayMovie,DesmumeFrame::playMovie)
EVT_MENU(wStopMovie,DesmumeFrame::stopMovie)
EVT_MENU(wRecordMovie,DesmumeFrame::recordMovie)

EVT_MENU(w3dView,DesmumeFrame::_3dView)

EVT_MENU(wLuaWindow,DesmumeFrame::OnOpenLuaWindow)

EVT_MENU(wConfigureControls,DesmumeFrame::OnOpenControllerConfiguration)

END_EVENT_TABLE()

IMPLEMENT_APP(Desmume)

bool Desmume::OnInit()
{
	if ( !wxApp::OnInit() )
		return false;

	NDS_Init ();

	Desmume_InitOnce();
	aggDraw.hud->attach((u8*)GPU_screen, 256, 384, 1024);//TODO

#ifdef __WIN32__
	extern void OpenConsole();
	OpenConsole();
#endif

	DesmumeFrame *frame = new DesmumeFrame((wxChar *)EMU_DESMUME_NAME_AND_VERSION());
	frame->Show(true);

	char *p, *a;
	std::string b = wxStandardPaths::Get().GetExecutablePath();
	a = const_cast<char*>(b.c_str());
	p = a + lstrlen(a);
	while (p >= a && *p != '\\') p--;
	if (++p >= a) *p = 0;

	executableDirectory = std::string(a);
	SPADInitialize PADInitialize;
	PADInitialize.padNumber = 1;
	extern void Initialize(void *init);
	Initialize(&PADInitialize);

	//TODO
	addon_type = NDS_ADDON_NONE;
	addonsChangePak(addon_type);

	return true;
}

DesmumeFrame::DesmumeFrame(const wxString& title)
: wxFrame(NULL, wxID_ANY, title)
{

	this->SetClientSize(256,384);

	wxMenu *fileMenu = new wxMenu;
	wxMenu *emulationMenu = new wxMenu;
	wxMenu *viewMenu = new wxMenu;
	wxMenu *configMenu = new wxMenu;
	wxMenu *toolsMenu = new wxMenu;
	wxMenu *helpMenu = new wxMenu;

	fileMenu->Append(wRom, (wxChar *)"Load R&om\tAlt-R");
	fileMenu->Append(wCloseRom, (wxChar *)"Close Rom");
	fileMenu->AppendSeparator();
	fileMenu->Append(wSaveStateAs, (wxChar *)"Save State As...");
	fileMenu->Append(wLoadStateFrom, (wxChar *)"Load State From...");
	{
		wxMenu* saves(DesmumeFrame::MakeStatesSubMenu(wSaveState01));
		wxMenu* loads(DesmumeFrame::MakeStatesSubMenu(wLoadState01));
		fileMenu->AppendSubMenu(saves, (wxChar *)"Save State");
		fileMenu->AppendSubMenu(loads, (wxChar *)"Load State");
#define ConnectMenuRange( id_start, inc, handler ) \
	Connect( id_start, id_start + inc, wxEVT_COMMAND_MENU_SELECTED, wxCommandEventHandler(DesmumeFrame::handler) )
		//TODO something is wrong here
//		ConnectMenuRange(wLoadState01+1, 10, Menu_LoadStates);
//		ConnectMenuRange(wSaveState01+1, 10, Menu_SaveStates);
	}
	fileMenu->AppendSeparator();
	fileMenu->Append(wImportBackupMemory, (wxChar *)"Import Backup Memory...");
	fileMenu->Append(wExportBackupMemory, (wxChar *)"Export Backup Memory...");
	fileMenu->AppendSeparator();
	fileMenu->Append(wSaveScreenshotAs, (wxChar *)"Save Screenshot As");
	fileMenu->Append(wQuickScreenshot, (wxChar *)"Quick Screenshot");
	fileMenu->AppendSeparator();
	fileMenu->Append(wPlayMovie, (wxChar *)"Play Movie");
	fileMenu->Append(wRecordMovie, (wxChar *)"Record Movie");
	fileMenu->Append(wStopMovie, (wxChar *)"Stop Movie");
	fileMenu->AppendSeparator();
	fileMenu->Append(wLuaWindow, (wxChar *)"New Lua Script Window...");
	fileMenu->AppendSeparator();
	fileMenu->Append(wExit, (wxChar *)"E&xit\tAlt-X", (wxChar *)"Quit this program");

	emulationMenu->Append(wPause, (wxChar *)"&Pause\tAlt-P", (wxChar *)"Pause Emulation");
	emulationMenu->Append(wReset, (wxChar *)"&Reset\tAlt-R", (wxChar *)"Reset Emulation");

	viewMenu->AppendSeparator();
	viewMenu->Append(wFrameCounter, (wxChar *)"&Display Frame Counter");
	viewMenu->Append(wFPS, (wxChar *)"&Display FPS");
	viewMenu->Append(wDisplayInput, (wxChar *)"&Display Input");
	viewMenu->Append(wDisplayGraphicalInput, (wxChar *)"&Display Graphical Input");
	viewMenu->Append(wDisplayLagCounter, (wxChar *)"&Display Lag Counter");
	viewMenu->Append(wDisplayMicrophone, (wxChar *)"&Display Microphone");

	toolsMenu->Append(w3dView, (wxChar *)"&3d Viewer");
	wxMenu *layersMenu = new wxMenu;
	{
		layersMenu->AppendCheckItem(wMainGPU, (wxChar *)"Main GPU");
		layersMenu->Append(wMainBG0, (wxChar *)"Main BG 0");
		layersMenu->Append(wMainBG1, (wxChar *)"Main BG 1");
		layersMenu->Append(wMainBG2, (wxChar *)"Main BG 2");
		layersMenu->Append(wMainBG3, (wxChar *)"Main BG 3");
		layersMenu->AppendSeparator();
		layersMenu->Append(wSubGPU, (wxChar *)"Sub GPU");
		layersMenu->Append(wSubBG0, (wxChar *)"Sub BG 0");
		layersMenu->Append(wSubBG1, (wxChar *)"Sub BG 1");
		layersMenu->Append(wSubBG2, (wxChar *)"Sub BG 2");
		layersMenu->Append(wSubBG3, (wxChar *)"Sub BG 3");
	}

	configMenu->Append(wConfigureControls, (wxChar *)"Controls");

	toolsMenu->AppendSeparator();
	toolsMenu->AppendSubMenu(layersMenu, (wxChar *)"View Layers");

	helpMenu->Append(wWebsite, (wxChar *)"&Website");
	helpMenu->Append(wForums, (wxChar *)"&Forums");
	helpMenu->Append(wSubmitABugReport, (wxChar *)"&Submit A Bug Report");
	helpMenu->Append(wAbout, (wxChar *)"&About", (wxChar *)"Show about dialog");

	wxMenuBar *menuBar = new wxMenuBar();
	menuBar->Append(fileMenu, (wxChar *)"&File");
	menuBar->Append(emulationMenu, (wxChar *)"&Emulation");
	menuBar->Append(viewMenu, (wxChar *)"&View");
	menuBar->Append(configMenu, (wxChar *)"&Config");
	menuBar->Append(toolsMenu, (wxChar *)"&Tools");
	menuBar->Append(helpMenu, (wxChar *)"&Help");
	SetMenuBar(menuBar);

	//	CreateStatusBar(2);
	//	SetStatusText("Welcome to Desmume!");
}
