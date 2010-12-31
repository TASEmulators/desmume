
#include "wxMain.h"
#include "NDSSystem.h"
#include "GPU_osd.h"
#include <wx/wxprec.h>
#include "gfx3d.h"
#include "version.h"
#include "addons.h"
#include "saves.h"
#include "movie.h"
#include "sndsdl.h"
#include "render3D.h"
#include "rasterize.h"
#include "OGLRender.h"
#include "firmware.h"
#ifdef HAVE_LIBAGG
#include "aggdraw.h"
#endif

#ifndef WIN32
#define lstrlen(a) strlen((a))
#endif

#include <wx/stdpaths.h>

#include "LuaWindow.h"
#include "PadSimple/GUI/ConfigDlg.h"
#include "PadSimple/pluginspecs_pad.h"
#include "wxdlg/wxcontrolsconfigdialog.h"

#ifndef WX_PRECOMP
#include <wx/wx.h>
#endif

#ifdef GDB_STUB
#include "gdbstub.h"
#endif
#include <wx/config.h>
#include <wx/docview.h>

#define SCREEN_SIZE (256*192*3)
#define GAP_DEFAULT 64
#define GAP_MAX	90
static int nds_gap_size;
static SPADInitialize PADInitialize;
static bool Touch = false;

SoundInterface_struct *SNDCoreList[] = {
        &SNDDummy,
        &SNDSDL,
        NULL
};

GPU3DInterface *core3DList[] = {
        &gpu3DRasterize,
#if !defined(WIN32) && !defined(__APPLE__)
        &gpu3Dgl,
#endif
        &gpu3DNull,
		NULL
};

volatile bool execute = false;

bool Desmume::OnInit()
{

	if ( !wxApp::OnInit() )
		return false;
	

#ifdef WIN32
	extern void OpenConsole();
	OpenConsole();
#endif

	SetAppName(_T("desmume")); 
	wxString iniFileName = wxPathOnly(wxStandardPaths::Get().GetExecutablePath()) + _T("\\desmume.ini");
	wxConfigBase *pConfig = new wxFileConfig(wxEmptyString,wxEmptyString,iniFileName,wxEmptyString,wxCONFIG_USE_LOCAL_FILE | wxCONFIG_USE_RELATIVE_PATH);
	wxConfigBase::Set(pConfig);
	wxString emu_version(EMU_DESMUME_NAME_AND_VERSION(), wxConvUTF8);
	DesmumeFrame *frame = new DesmumeFrame(emu_version);
	frame->NDSInitialize();

	frame->Show(true);

	PADInitialize.padNumber = 1;

#ifndef WIN32
	extern void Initialize(void *init);

	Initialize(&PADInitialize);
#endif

	return true;
}


void DesmumeFrame::NDSInitialize() {
	NDS_FillDefaultFirmwareConfigData( &fw_config);

#ifdef HAVE_LIBAGG
	Desmume_InitOnce();
	aggDraw.hud->attach((u8*)GPU_screen, 256, 384, 1024);//TODO
#endif

	//TODO
	addon_type = NDS_ADDON_NONE;
	addonsChangePak(addon_type);

#ifdef GDB_STUB
	arm9_memio = &arm9_base_memory_iface;
	arm7_memio = &arm7_base_memory_iface;
	NDS_Init(	arm9_memio, &arm9_ctrl_iface,
				arm7_memio, &arm7_ctrl_iface);
#else
	NDS_Init();
#endif
	SPU_ChangeSoundCore(SNDCORE_SDL, 735 * 4);
	NDS_3D_ChangeCore(0);
	NDS_CreateDummyFirmware( &fw_config);
}

DesmumeFrame::DesmumeFrame(const wxString& title)
: wxFrame(NULL, wxID_ANY, title)
{
	history = new wxFileHistory;
	wxMenuBar *menuBar = new wxMenuBar();
	wxMenu *fileMenu = new wxMenu;
	wxMenu *emulationMenu = new wxMenu;
	wxMenu *viewMenu = new wxMenu;
	wxMenu *configMenu = new wxMenu;
	wxMenu *toolsMenu = new wxMenu;
	wxMenu *helpMenu = new wxMenu;
	wxMenu *recentMenu = new wxMenu;
	wxMenu *saves(MakeStatesSubMenu(wSaveState01));
	wxMenu *loads(MakeStatesSubMenu(wLoadState01));
	wxMenu *layersMenu = new wxMenu;
	wxMenu *rotateMenu = new wxMenu;
	
	LoadSettings();

	menuBar->Append(fileMenu, _T("&File"));
	menuBar->Append(emulationMenu, _T("&Emulation"));
	menuBar->Append(viewMenu, _T("&View"));
	menuBar->Append(configMenu, _T("&Config"));
	menuBar->Append(toolsMenu, _T("&Tools"));
	menuBar->Append(helpMenu, _T("&Help"));

	fileMenu->Append(wxID_OPEN, _T("Load R&om\tAlt-R"));
	fileMenu->AppendSubMenu(recentMenu, _T("Recent files"));
	fileMenu->Append(wCloseRom, _T("Close Rom"));
	fileMenu->AppendSeparator();
	fileMenu->Append(wSaveStateAs, _T("Save State As..."));
	fileMenu->Append(wLoadStateFrom, _T("Load State From..."));
	fileMenu->AppendSubMenu(saves, _T("Save State"));
	fileMenu->AppendSubMenu(loads, _T("Load State"));
	fileMenu->AppendSeparator();
	fileMenu->Append(wImportBackupMemory, _T("Import Backup Memory..."));
	fileMenu->Append(wExportBackupMemory, _T("Export Backup Memory..."));
	fileMenu->AppendSeparator();
	fileMenu->Append(wSaveScreenshotAs, _T("Save Screenshot As"));
	fileMenu->Append(wQuickScreenshot, _T("Quick Screenshot"));
	fileMenu->AppendSeparator();
	fileMenu->Append(wPlayMovie, _T("Play Movie"));
	fileMenu->Append(wRecordMovie, _T("Record Movie"));
	fileMenu->Append(wStopMovie, _T("Stop Movie"));
	fileMenu->AppendSeparator();
	fileMenu->Append(wLuaWindow, _T("New Lua Script Window..."));
	fileMenu->AppendSeparator();
	fileMenu->Append(wxID_EXIT, _T("E&xit\tAlt-X"), _T("Quit this program"));

	emulationMenu->Append(wPause, _T("&Pause\tAlt-P"), _T("Pause Emulation"));
	emulationMenu->Append(wReset, _T("&Reset\tAlt-R"), _T("Reset Emulation"));
	
	configMenu->Append(wConfigureControls, _T("Controls"));

	toolsMenu->Append(w3dView, _T("&3d Viewer"));
	toolsMenu->AppendSeparator();
	toolsMenu->AppendSubMenu(layersMenu, _T("View Layers"));

	layersMenu->AppendCheckItem(wMainGPU, _T("Main GPU"));
	layersMenu->Check(wMainGPU, true);
	layersMenu->AppendCheckItem(wMainBG0, _T("Main BG 0"));
	layersMenu->Check(wMainBG0, true);
	layersMenu->AppendCheckItem(wMainBG1, _T("Main BG 1"));
	layersMenu->Check(wMainBG1, true);
	layersMenu->AppendCheckItem(wMainBG2, _T("Main BG 2"));
	layersMenu->Check(wMainBG2, true);
	layersMenu->AppendCheckItem(wMainBG3, _T("Main BG 3"));
	layersMenu->Check(wMainBG3, true);
	layersMenu->AppendSeparator();
	layersMenu->AppendCheckItem(wSubGPU, _T("Sub GPU"));
	layersMenu->Check(wSubGPU, true);
	layersMenu->AppendCheckItem(wSubBG0, _T("Sub BG 0"));
	layersMenu->Check(wSubBG0, true);
	layersMenu->AppendCheckItem(wSubBG1, _T("Sub BG 1"));
	layersMenu->Check(wSubBG1, true);
	layersMenu->AppendCheckItem(wSubBG2, _T("Sub BG 2"));
	layersMenu->Check(wSubBG2, true);
	layersMenu->AppendCheckItem(wSubBG3, _T("Sub BG 3"));
	layersMenu->Check(wSubBG3, true);

	helpMenu->Append(wWebsite, _T("&Website"));
	helpMenu->Append(wForums, _T("&Forums"));
	helpMenu->Append(wSubmitABugReport, _T("&Submit A Bug Report"));
	helpMenu->Append(wxID_ABOUT);
	
	viewMenu->AppendSubMenu(rotateMenu, _T("Rotate"));
	viewMenu->AppendSeparator();
	viewMenu->Append(wSetHUDFont, _T("&Set HUD Font"));
	viewMenu->AppendCheckItem(wFrameCounter, _T("&Display Frame Counter"));
	viewMenu->AppendCheckItem(wFPS, _T("&Display FPS"));
	viewMenu->AppendCheckItem(wDisplayInput, _T("&Display Input"));
	viewMenu->AppendCheckItem(wDisplayGraphicalInput, _T("&Display Graphical Input"));
	viewMenu->AppendCheckItem(wDisplayLagCounter, _T("&Display Lag Counter"));
	viewMenu->AppendCheckItem(wDisplayMicrophone, _T("&Display Microphone"));

	rotateMenu->AppendRadioItem(wRot0, _T("0"));
	rotateMenu->AppendRadioItem(wRot90, _T("90"));
	rotateMenu->AppendRadioItem(wRot180, _T("180"));
	rotateMenu->AppendRadioItem(wRot270, _T("270"));
	rotateMenu->Check(wRot0+(nds_screen_rotation_angle/90), true);

	/*
	loadfileMenu(fileMenu);
	loademulationMenu(emulationMenu);
	loadconfigMenu(configMenu);
	loadtoolsMenu(toolsMenu);
	loadhelpMenu(helpMenu);
	loadviewMenu(viewMenu);

	loadmenuBar(menuBar);
	*/
	ChangeRotation(nds_screen_rotation_angle, false);
	SetMenuBar(menuBar);
}

void DesmumeFrame::OnQuit(wxCommandEvent& WXUNUSED(event))
{
	execute = false;
	NDS_DeInit();
	Close(true);
}
void DesmumeFrame::OnAbout(wxCommandEvent& WXUNUSED(event))
{
	wxMessageBox(
		wxString::Format(wxT("Desmume on %s"),wxGetOsDescription().c_str()),
		wxT("About Desmume"),
		wxOK | wxICON_INFORMATION,
		this);
}

void DesmumeFrame::applyInput()
{

	bool up,down,left,right,x,y,a,b,l,r,start,select;

	up = down = left = right = x = y = a = b = l = r = start = select = false;

	SPADStatus s;
	memset(&s,0,sizeof(s));

	//TODO !!!!!!!!!!!!!!!!!!!!!! FIXME!!!!!!!!!!1
#ifndef _MSC_VER
//		PAD_GetStatus(0, &s);
#endif

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

void DesmumeFrame::LoadRom(wxCommandEvent& event)
{
	wxFileDialog dialog(this,_T("Load Rom"),wxGetHomeDir(),_T(""),_T("*.nds"),wxFD_OPEN, wxDefaultPosition, wxDefaultSize);
	if(dialog.ShowModal() == wxID_OK) {
		history->AddFileToHistory(dialog.GetPath());
		execute = true;
		NDS_LoadROM(dialog.GetPath().mb_str(), dialog.GetPath().mb_str());
	}
}
bool DesmumeFrame::DetransformTouchCoords(int& X, int& Y)
{
	int dtX, dtY;
	
	// TODO: descaling (when scaling is supported)

	// De-rotate coordinates
	switch (nds_screen_rotation_angle)
	{
	case 0: dtX = X; dtY = Y - 191 - nds_gap_size; break;
	case 90: dtX = Y; dtY = 191 - X; break;
	case 180: dtX = 255 - X; dtY = 191 - Y; break;
	case 270: dtX = 255 - Y; dtY = X - 191 - nds_gap_size; break;
	}

	// Atleast one of the coordinates is out of range
	if ((dtX < 0) || (dtX > 255) || (dtY < 0) || (dtY > 191))
	{
		X = wxClip(dtX, 0, 255);
		Y = wxClip(dtY, 0, 191);
		return false;
	}
	else
	{
		X = dtX;
		Y = dtY;
		return true;
	}
}
void DesmumeFrame::OnTouchEvent(wxMouseEvent& evt)
{
	wxPoint pt = evt.GetPosition();
	bool inside = DetransformTouchCoords(pt.x, pt.y);

	if (evt.LeftDown() && inside)
	{
		Touch = true;
		NDS_setTouchPos((u16)pt.x, (u16)pt.y);
	}
	else if(evt.LeftUp() && Touch)
	{
		Touch = false;
		NDS_releaseTouch();
	}
	else if (Touch)
	{
		NDS_setTouchPos((u16)pt.x, (u16)pt.y);
	}
}
void DesmumeFrame::gpu_screen_to_rgb(u8 *rgb1, u8 *rgb2)
{
	u16 gpu_pixel;
	u8 pixel[3];
	u8 *rgb = rgb1;
	const int rot = nds_screen_rotation_angle;
	int done = false;
	int offset = 0;

loop:
	for (int i = 0; i < 256; i++) {
		for (int j = 0; j < 192; j++) {
			gpu_pixel = *((u16 *) & GPU_screen[(i + (j + offset) * 256) << 1]);
			pixel[0] = ((gpu_pixel >> 0) & 0x1f) << 3;
			pixel[1] = ((gpu_pixel >> 5) & 0x1f) << 3;
			pixel[2] = ((gpu_pixel >> 10) & 0x1f) << 3;
			switch (rot) {
			case 0:
				memcpy(rgb+((i+j*256)*3),pixel,3);
				break;
			case 90:
				memcpy(rgb+SCREEN_SIZE-((j+(255-i)*192)*3)-3,pixel,3);
				break;
			case 180:
				memcpy(rgb+SCREEN_SIZE-((i+j*256)*3)-3,pixel,3);
				break;
			case 270:
				memcpy(rgb+((j+(255-i)*192)*3),pixel,3);
				break;
			}
		}

	}

	if (done == false) {
		offset = 192;
		rgb = rgb2;
		done = true;
		goto loop;
	}
}
void DesmumeFrame::onPaint(wxPaintEvent &event)
{
	u8 rgb1[SCREEN_SIZE], rgb2[SCREEN_SIZE];
	wxPaintDC dc(this);
	int w, h;

	if (nds_screen_rotation_angle == 90 || nds_screen_rotation_angle == 270) {
		w = 192;
		h = 256;
	} else {
		w = 256;
		h = 192;
	}

	gpu_screen_to_rgb(rgb1, rgb2);
	wxBitmap m_bitmap1(wxImage(w, h, rgb1, true));
	wxBitmap m_bitmap2(wxImage(w, h, rgb2, true));
	switch (nds_screen_rotation_angle) {
	case 0:
		dc.DrawBitmap(m_bitmap1, 0, 0, true);
		dc.DrawBitmap(m_bitmap2, 0, 192+nds_gap_size, true);
		break;
	case 90:
		dc.DrawBitmap(m_bitmap2, 0, 0, true);
		dc.DrawBitmap(m_bitmap1, 192+nds_gap_size, 0, true);
		break;
	case 180:
		dc.DrawBitmap(m_bitmap2, 0, 0, true);
		dc.DrawBitmap(m_bitmap1, 0, 192+nds_gap_size, true);
		break;
	case 270:
		dc.DrawBitmap(m_bitmap1, 0, 0, true);
		dc.DrawBitmap(m_bitmap2, 192+nds_gap_size, 0, true);
		break;
	}
}



void DesmumeFrame::onIdle(wxIdleEvent &event)
{
	Refresh(false);
	event.RequestMore();
	if (execute)
	{
		applyInput();
		NDS_exec<false>();
		SPU_Emulate_user();
		osd->update();
		DrawHUD();
		osd->clear();
	}
	else
		wxMilliSleep(250);
	// wxMicroSleep(16.7*1000);
}

void DesmumeFrame::pause(wxCommandEvent& event)
{
	if (execute) {
		execute=false;
		SPU_Pause(1);
	} else {
		execute=true;
		SPU_Pause(0);
	}
}
void DesmumeFrame::frameCounter(wxCommandEvent& event)
{
	CommonSettings.hud.FrameCounterDisplay ^= true;
	osd->clear();
}
void DesmumeFrame::FPS(wxCommandEvent& event)
{
	CommonSettings.hud.FpsDisplay ^= true;
	osd->clear();
}

void DesmumeFrame::displayInput(wxCommandEvent& event)
{
	CommonSettings.hud.ShowInputDisplay ^= true;
	osd->clear();
}

void DesmumeFrame::displayGraphicalInput(wxCommandEvent& event)
{
	CommonSettings.hud.ShowGraphicalInputDisplay ^= true;
	osd->clear();
}
void DesmumeFrame::displayLagCounter(wxCommandEvent& event)
{
	CommonSettings.hud.ShowLagFrameCounter ^= true;
	osd->clear();
}
void DesmumeFrame::displayMicrophone(wxCommandEvent& event)
{
	CommonSettings.hud.ShowMicrophone ^= true;
	osd->clear();
}
#ifdef HAVE_LIBAGG
void DesmumeFrame::setHUDFont(wxCommandEvent &event)
{
	wxArrayString fontchoices;
	for(int i = 0; i < font_Nums;i++)
	{
		fontchoices.Add(fonts_list[i].name);
	}

	wxSingleChoiceDialog fontDialog(this,
							_T("Please select a font for use on the HUD:\n"),
							_T("Configure HUD font"),
							 fontchoices);
	fontDialog.SetSelection(fontchoices.GetCount() - 1);
	if(fontDialog.ShowModal() == wxID_OK)
	{
		aggDraw.hud->setFont(fonts_list[fontDialog.GetSelection()].name);
	}
	
}
#endif
void DesmumeFrame::mainG(int n) 
{
	if(CommonSettings.dispLayers[0][n])
		GPU_remove(MainScreen.gpu, n);
	else
		GPU_addBack(MainScreen.gpu, n);	
}
void DesmumeFrame::subG(int n) 
{
	if(CommonSettings.dispLayers[1][n])
		GPU_remove(SubScreen.gpu, n);
	else
		GPU_addBack(SubScreen.gpu, n);	
}
void DesmumeFrame::_3dView(wxCommandEvent& event) 
{
	driver->VIEW3D_Init();
	driver->view3d->Launch();
}
void DesmumeFrame::saveStateAs(wxCommandEvent& event) 
{
	wxFileDialog dialog(this,_T("Save State As"),wxGetHomeDir(),_T(""),_T("*.dst"),wxFD_SAVE, wxDefaultPosition, wxDefaultSize);
	if(dialog.ShowModal() == wxID_OK)
		savestate_save (dialog.GetPath().mb_str());
}
void DesmumeFrame::loadStateFrom(wxCommandEvent& event) 
{
	wxFileDialog dialog(this,_T("Load State From"),wxGetHomeDir(),_T(""),_T("*.dst"),wxFD_OPEN, wxDefaultPosition, wxDefaultSize);
	if(dialog.ShowModal() == wxID_OK)
		savestate_load (dialog.GetPath().mb_str());
}
void DesmumeFrame::closeRom(wxCommandEvent& event) 
{
	NDS_FreeROM();
	execute = false;
	SPU_Pause(1);
#ifdef HAVE_LIBAGG
	Hud.resetTransient();
#endif
	NDS_Reset();
}
void DesmumeFrame::importBackupMemory(wxCommandEvent& event)
{
	wxFileDialog dialog(this,_T("Import Backup Memory"),wxGetHomeDir(),_T(""),_T("*.duc, *.sav"),wxFD_OPEN, wxDefaultPosition, wxDefaultSize);
	if(dialog.ShowModal() == wxID_OK)
		if (!NDS_ImportSave(dialog.GetPath().mb_str()))
			wxMessageBox(wxString::Format(_T("Save was not successfully imported")),_T("Error"),wxOK | wxICON_ERROR,this);
}
void DesmumeFrame::exportBackupMemory(wxCommandEvent& event)
{
	wxFileDialog dialog(this,_T("Export Backup Memory"),wxGetHomeDir(),_T(""),_T("*.duc, *.sav"),wxFD_SAVE, wxDefaultPosition, wxDefaultSize);
	if(dialog.ShowModal() == wxID_OK)
		if (!NDS_ExportSave(dialog.GetPath().mb_str()))
			wxMessageBox(wxString::Format(_T("Save was not successfully exported")),_T("Error"),wxOK | wxICON_ERROR,this);
}
void DesmumeFrame::saveScreenshotAs(wxCommandEvent& event) 
{
	wxFileDialog dialog(this,_T("Save Screenshot As"),wxGetHomeDir(),_T(""),_T("*.png"),wxFD_SAVE, wxDefaultPosition, wxDefaultSize);
	if(dialog.ShowModal() == wxID_OK)
		NDS_WritePNG(dialog.GetPath().mb_str());
}
void DesmumeFrame::quickScreenshot(wxCommandEvent& event)
{
		NDS_WritePNG(wxStandardPaths::Get().GetExecutablePath().mb_str());//TODO GetExecutablePath is wrong
}
void DesmumeFrame::OnOpenLuaWindow(wxCommandEvent& WXUNUSED (event))
{
#ifdef WIN32
	new wxLuaWindow(this, wxDefaultPosition, wxSize(600, 390));
#endif
}
void DesmumeFrame::OnOpenControllerConfiguration(wxCommandEvent& WXUNUSED (event))
{
//#ifndef _MSC_VER
//	new PADConfigDialogSimple(this);
	(new wxControlsConfigDialog(this))->ShowModal();
//#endif
}

void DesmumeFrame::Menu_SaveStates(wxCommandEvent &event){savestate_slot(event.GetId() - wSaveState01);}
void DesmumeFrame::Menu_LoadStates(wxCommandEvent &event){loadstate_slot(event.GetId() - wLoadState01);}

void DesmumeFrame::OnRotation(wxCommandEvent &event) {
	ChangeRotation((event.GetId() - wRot0)*90, true);
}

void DesmumeFrame::ChangeRotation(int rot, bool skip) {
	wxSize sizeMin, sizeMax;

	if (skip && rot == nds_screen_rotation_angle)
		return;

	SetMinSize(wxSize(-1,-1));
	SetMaxSize(wxSize(-1,-1));

	if (rot == 90 || rot == 270) {
		SetClientSize(384 + nds_gap_size, 256);
		sizeMax = sizeMin = ClientToWindowSize(wxSize(384,256));
		sizeMax.IncBy(GAP_MAX,0);
	} else {
		SetClientSize(256, 384 + nds_gap_size);
		sizeMax = sizeMin = ClientToWindowSize(wxSize(256,384));
		sizeMax.IncBy(0,GAP_MAX);
	}
	SetMinSize(sizeMin);
	SetMaxSize(sizeMax);
	nds_screen_rotation_angle = rot;
}

void DesmumeFrame::onResize(wxSizeEvent &event) {
	int w, h;

	GetClientSize(&w,&h);
	if (nds_screen_rotation_angle == 90 || nds_screen_rotation_angle == 270) {
		if (w>=384)
			nds_gap_size = w-384;
	} else {
		if (h>=384)
			nds_gap_size = h-384;
	}
	event.Skip();
}

BEGIN_EVENT_TABLE(DesmumeFrame, wxFrame)

	EVT_PAINT(DesmumeFrame::onPaint)
	EVT_IDLE(DesmumeFrame::onIdle)
	EVT_SIZE(DesmumeFrame::onResize)
	EVT_LEFT_DOWN(DesmumeFrame::OnTouchEvent)
	EVT_LEFT_UP(DesmumeFrame::OnTouchEvent)
	EVT_LEFT_DCLICK(DesmumeFrame::OnTouchEvent)
	EVT_MOTION(DesmumeFrame::OnTouchEvent)
	EVT_CLOSE(DesmumeFrame::OnClose)

	EVT_MENU(wxID_EXIT, DesmumeFrame::OnQuit)
	EVT_MENU(wxID_OPEN, DesmumeFrame::LoadRom)
	EVT_MENU(wxID_ABOUT,DesmumeFrame::OnAbout)
	EVT_MENU(wPause,DesmumeFrame::pause)
	EVT_MENU(wReset,DesmumeFrame::reset)
	EVT_MENU(wFrameCounter,DesmumeFrame::frameCounter)
	EVT_MENU(wFPS,DesmumeFrame::FPS)
	EVT_MENU(wDisplayInput,DesmumeFrame::displayInput)
	EVT_MENU(wDisplayGraphicalInput,DesmumeFrame::displayGraphicalInput)
	EVT_MENU(wDisplayLagCounter,DesmumeFrame::displayLagCounter)
	EVT_MENU(wDisplayMicrophone,DesmumeFrame::displayMicrophone)
#ifdef HAVE_LIBAGG
	EVT_MENU(wSetHUDFont,DesmumeFrame::setHUDFont)
#endif

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

	EVT_MENU_RANGE(wSaveState01,wSaveState01+9,DesmumeFrame::Menu_SaveStates)
	EVT_MENU_RANGE(wLoadState01,wLoadState01+9,DesmumeFrame::Menu_LoadStates)

	EVT_MENU(wCloseRom,DesmumeFrame::closeRom)
	EVT_MENU(wImportBackupMemory,DesmumeFrame::importBackupMemory)
	EVT_MENU(wExportBackupMemory,DesmumeFrame::exportBackupMemory)

	EVT_MENU_RANGE(wRot0,wRot270,DesmumeFrame::OnRotation)

	EVT_MENU(wSaveScreenshotAs,DesmumeFrame::saveScreenshotAs)
	EVT_MENU(wQuickScreenshot,DesmumeFrame::quickScreenshot)

	EVT_MENU(wPlayMovie,DesmumeFrame::playMovie)
	EVT_MENU(wStopMovie,DesmumeFrame::stopMovie)
	EVT_MENU(wRecordMovie,DesmumeFrame::recordMovie)

	EVT_MENU(w3dView,DesmumeFrame::_3dView)

	EVT_MENU(wLuaWindow,DesmumeFrame::OnOpenLuaWindow)

	EVT_MENU(wConfigureControls,DesmumeFrame::OnOpenControllerConfiguration)

	EVT_MENU_RANGE(wxID_FILE1,wxID_FILE9,DesmumeFrame::OnOpenRecent)

END_EVENT_TABLE()

IMPLEMENT_APP(Desmume)


#ifdef WIN32
/*
* The thread handling functions needed by the GDB stub code.
*/
void *
createThread_gdb( void (APIENTRY *thread_function)( void *data),
				 void *thread_data) {
					 void *new_thread = CreateThread( NULL, 0,
						 (LPTHREAD_START_ROUTINE)thread_function, thread_data,
						 0, NULL);

					 return new_thread;
}

void
joinThread_gdb( void *thread_handle) {
}
#endif
bool DesmumeFrame::LoadSettings() {
	wxConfigBase::Get()->Read(_T("/Screen/Gap"),&nds_gap_size,0);
	wxConfigBase::Get()->Read(_T("/Screen/Rotation"),&nds_screen_rotation_angle,0);
	wxConfigBase::Get()->SetPath(_T("/History"));
	history->Load(*wxConfigBase::Get());
	return true;
}

bool DesmumeFrame::SaveSettings() {
	wxConfigBase::Get()->Write(_T("/Screen/Gap"),nds_gap_size);
	wxConfigBase::Get()->Write(_T("/Screen/Rotation"),nds_screen_rotation_angle);
	wxConfigBase::Get()->SetPath(_T("/History"));
	history->Save(*wxConfigBase::Get());
	return true;
}

void DesmumeFrame::OnClose(wxCloseEvent &event) {
	SaveSettings();
	event.Skip();
}

void DesmumeFrame::OnOpenRecent(wxCommandEvent &event) {
	int ret;
	size_t id = event.GetId()-wxID_FILE1;

	ret = NDS_LoadROM(history->GetHistoryFile(id).mb_str(), history->GetHistoryFile(id).mb_str());
	if (ret > 0)
		execute = true;
	else
		history->RemoveFileFromHistory(id);
}


void DesmumeFrame::loadfileMenu(wxMenu *fileMenu)
{
	wxMenu *recentMenu = new wxMenu;
	wxMenu *saves(MakeStatesSubMenu(wSaveState01));
	wxMenu *loads(MakeStatesSubMenu(wLoadState01));
	fileMenu->Append(wxID_OPEN, _T("Load R&om\tAlt-R"));
	fileMenu->AppendSubMenu(recentMenu, _T("Recent files"));
	fileMenu->Append(wCloseRom, _T("Close Rom"));
	fileMenu->AppendSeparator();
	fileMenu->Append(wSaveStateAs, _T("Save State As..."));
	fileMenu->Append(wLoadStateFrom, _T("Load State From..."));
	fileMenu->AppendSubMenu(saves, _T("Save State"));
	fileMenu->AppendSubMenu(loads, _T("Load State"));
	fileMenu->AppendSeparator();
	fileMenu->Append(wImportBackupMemory, _T("Import Backup Memory..."));
	fileMenu->Append(wExportBackupMemory, _T("Export Backup Memory..."));
	fileMenu->AppendSeparator();
	fileMenu->Append(wSaveScreenshotAs, _T("Save Screenshot As"));
	fileMenu->Append(wQuickScreenshot, _T("Quick Screenshot"));
	fileMenu->AppendSeparator();
	fileMenu->Append(wPlayMovie, _T("Play Movie"));
	fileMenu->Append(wRecordMovie, _T("Record Movie"));
	fileMenu->Append(wStopMovie, _T("Stop Movie"));
	fileMenu->AppendSeparator();
	fileMenu->Append(wLuaWindow, _T("New Lua Script Window..."));
	fileMenu->AppendSeparator();
	fileMenu->Append(wxID_EXIT, _T("E&xit\tAlt-X"), _T("Quit this program"));
	
//	history->UseMenu(recentMenu);
}
void DesmumeFrame::loademulationMenu(wxMenu *emulationMenu)
{
	emulationMenu->Append(wPause, _T("&Pause\tAlt-P"), _T("Pause Emulation"));
	emulationMenu->Append(wReset, _T("&Reset\tAlt-R"), _T("Reset Emulation"));
}
void DesmumeFrame::loadrotateMenu(wxMenu *rotateMenu)
{
	rotateMenu->AppendRadioItem(wRot0, _T("0"));
	rotateMenu->AppendRadioItem(wRot90, _T("90"));
	rotateMenu->AppendRadioItem(wRot180, _T("180"));
	rotateMenu->AppendRadioItem(wRot270, _T("270"));
	rotateMenu->Check(wRot0+(nds_screen_rotation_angle/90), true);
}
void DesmumeFrame::loadviewMenu(wxMenu *viewMenu)
{
	wxMenu *rotateMenu = new wxMenu;
	viewMenu->AppendSubMenu(rotateMenu, _T("Rotate"));
	viewMenu->AppendSeparator();
	viewMenu->Append(wSetHUDFont, _T("&Set HUD Font"));
	viewMenu->AppendCheckItem(wFrameCounter, _T("&Display Frame Counter"));
	viewMenu->AppendCheckItem(wFPS, _T("&Display FPS"));
	viewMenu->AppendCheckItem(wDisplayInput, _T("&Display Input"));
	viewMenu->AppendCheckItem(wDisplayGraphicalInput, _T("&Display Graphical Input"));
	viewMenu->AppendCheckItem(wDisplayLagCounter, _T("&Display Lag Counter"));
	viewMenu->AppendCheckItem(wDisplayMicrophone, _T("&Display Microphone"));
	loadrotateMenu(rotateMenu);
}
void DesmumeFrame::loadtoolsMenu(wxMenu *toolsMenu)
{
	wxMenu *layersMenu = new wxMenu;

	toolsMenu->Append(w3dView, _T("&3d Viewer"));
	toolsMenu->AppendSeparator();
	toolsMenu->AppendSubMenu(layersMenu, _T("View Layers"));
	
	loadlayersMenu(layersMenu);
}

void DesmumeFrame::loadlayersMenu(wxMenu *layersMenu)
{
	layersMenu->AppendCheckItem(wMainGPU, _T("Main GPU"));
	layersMenu->Check(wMainGPU, true);
	layersMenu->AppendCheckItem(wMainBG0, _T("Main BG 0"));
	layersMenu->Check(wMainBG0, true);
	layersMenu->AppendCheckItem(wMainBG1, _T("Main BG 1"));
	layersMenu->Check(wMainBG1, true);
	layersMenu->AppendCheckItem(wMainBG2, _T("Main BG 2"));
	layersMenu->Check(wMainBG2, true);
	layersMenu->AppendCheckItem(wMainBG3, _T("Main BG 3"));
	layersMenu->Check(wMainBG3, true);
	layersMenu->AppendSeparator();
	layersMenu->AppendCheckItem(wSubGPU, _T("Sub GPU"));
	layersMenu->Check(wSubGPU, true);
	layersMenu->AppendCheckItem(wSubBG0, _T("Sub BG 0"));
	layersMenu->Check(wSubBG0, true);
	layersMenu->AppendCheckItem(wSubBG1, _T("Sub BG 1"));
	layersMenu->Check(wSubBG1, true);
	layersMenu->AppendCheckItem(wSubBG2, _T("Sub BG 2"));
	layersMenu->Check(wSubBG2, true);
	layersMenu->AppendCheckItem(wSubBG3, _T("Sub BG 3"));
	layersMenu->Check(wSubBG3, true);
}
void DesmumeFrame::loadconfigMenu(wxMenu *configMenu)
{
	configMenu->Append(wConfigureControls, _T("Controls"));
}
void DesmumeFrame::loadhelpMenu(wxMenu *helpMenu)
{
	helpMenu->Append(wWebsite, _T("&Website"));
	helpMenu->Append(wForums, _T("&Forums"));
	helpMenu->Append(wSubmitABugReport, _T("&Submit A Bug Report"));
	helpMenu->Append(wxID_ABOUT);
}

void DesmumeFrame::loadmenuBar(wxMenuBar *menuBar)
{
	wxMenu *fileMenu = new wxMenu;
	wxMenu *emulationMenu = new wxMenu;
	wxMenu *viewMenu = new wxMenu;
	wxMenu *configMenu = new wxMenu;
	wxMenu *toolsMenu = new wxMenu;
	wxMenu *helpMenu = new wxMenu;

	menuBar->Append(fileMenu, _T("&File"));
	menuBar->Append(emulationMenu, _T("&Emulation"));
	menuBar->Append(viewMenu, _T("&View"));
	menuBar->Append(configMenu, _T("&Config"));
	menuBar->Append(toolsMenu, _T("&Tools"));
	menuBar->Append(helpMenu, _T("&Help"));

	loadfileMenu(fileMenu);
	loademulationMenu(emulationMenu);
	loadconfigMenu(configMenu);
	loadtoolsMenu(toolsMenu);
	loadhelpMenu(helpMenu);
	loadviewMenu(viewMenu);
	
}
