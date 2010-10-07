/*  Copyright 2010 Yoshihiro

    This file is part of Desmume360 for xbox360.

    Desmume360 is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    Desmume60 is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with Desmume360; if not, write to the Free Software
    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301  USA
*/
#include <xtl.h>
#include <VectorIntrinsics.h>
#include <process.h>
#include "d3d9.h"      // Note: make sure this is the Xbox 360 flavor of D3D9.h!
#include <d3dx9.h>
#include "xgraphics.h"
#include "d3dx9math.h"
#include "AtgApp.h"
#include "AtgFont.h"
#include "AtgHelp.h"
#include "AtgInput.h"
#include "AtgUtil.h"
#include "AtgSimpleShaders.h"
#include "AtgDebugDraw.h"
#include <iostream>
#include <stdlib.h>
#include "SDL.h"

#include "../gfx3d.h"

#include "../MMU.h"
#include "../NDSSystem.h"
#include "../firmware.h"
#include "../debug.h"
#include "../sndsdl.h"
#include "../render3D.h"
#include "../rasterize.h"
#include "../saves.h"

#include <D3dx9core.h>

#include "ctrls360.h"

WCHAR ExtFPS[1000];


volatile bool execute = FALSE;

static float nds_screen_size_ratio = 2.0f;

CRITICAL_SECTION    DsExec;

SoundInterface_struct *SNDCoreList[] = {
  &SNDDummy,
  &SNDDummy,
  &SNDSDL,
  NULL
};

GPU3DInterface *core3DList[] = {
&gpu3DNull,
&gpu3DRasterize,
NULL
};
static unsigned short keypad = 0;
u32 last_cycle = 0;
char rompath[256];
char statepath[256];
int romloaded = 0;
int paused = 0;

static int FrameLimit = 1;
bool frameAdvance = false;
int autoframeskipenab=1;
int frameskiprate=0;
int emu_paused = 0;

//--------------------------------------------------------------------------------------
// Color values
//--------------------------------------------------------------------------------------
const DWORD         INFO_COLOR = 0xffffff00;          // Yellow


#define IN_GAME						0
#define IN_GAME_PAUSED				1
#define MAIN_MENU					2
#define INIT_SYSTEM					3
#define INIT_MENU_SYSTEM		    4

class Sample : public ATG::Application
{
	ATG::Timer m_Timer;
    ATG::Font m_Font;
    LPDIRECT3DTEXTURE9		m_pCurrentImage;
	LPDIRECT3DTEXTURE9		newtex;
	LPDIRECT3DTEXTURE9		splashscreen;
	LPDIRECT3DTEXTURE9		mouseptr;
	LPDIRECT3DSURFACE9 m_pdBkBuffer;

    DWORD m_dwCurrentImageIndex;

	typedef struct _filenamestruct {
		WCHAR name[2000] ;
		unsigned char filename[2000] ;
	} FILENAME ;

	UINT32 topIdx;
	UINT32 curr;
	int spos;

	unsigned long numfiles;
	int		m_nXOffset, m_nFontHeight ;
	int		m_namesPerPage ;
	int		m_state;

	int		Skins; 
	UINT32 theWidth;
	UINT32 theHeight;
	FILENAME *files ;

public:
	HRESULT         DSInit();
	void			ExecEmu();
	HRESULT			InitializeWithScreen();
	void			MoveCursor();
	VOID			ScreenGrab(char *fileName);
	void			QuickSort( int lo, int hi );
	void			FindAvailRoms();
	void			DSPAD();
private:
    virtual HRESULT Initialize();
    virtual HRESULT Update();
    virtual HRESULT Render();
};

Sample atgApp;

struct mouse_status mouse;
HANDLE hMainThread;
int StopThread;


struct my_config {
  int disable_sound;

  int disable_limiter;

  int engine_3d;

  int frameskip;

  int firmware_language;

  int savetype;

  const char *nds_file;
  const char *cflash_disk_image_file;
};


const char * save_type_names[] = {
    "Autodetect",
    "EEPROM 4kbit",
    "EEPROM 64kbit",
    "EEPROM 512kbit",
    "FRAM 256kbit",
    "FLASH 2mbit",
    "FLASH 4mbit",
    NULL
};

static void
init_config( struct my_config *config) {
  config->disable_limiter = 0;

  config->engine_3d = 1;

  config->nds_file = NULL;

  config->frameskip = 0;

  config->cflash_disk_image_file = NULL;

  config->savetype = 0;

  /* use the default language */
  config->firmware_language = -1;

  mouse.xx = 0;
  mouse.yy = 0;
  mouse.x  = 0;
  mouse.y  = 0;
}


int savetype=MC_TYPE_AUTODETECT;
u32 savesize=1;


//--------------------------------------------------------------------------------------
// Name: main()
// Desc: Entry point to the program
//--------------------------------------------------------------------------------------
VOID __cdecl main()
{
	atgApp.m_d3dpp.BackBufferWidth  = 640;
	atgApp.m_d3dpp.BackBufferHeight = 480;

	atgApp.m_d3dpp.BackBufferFormat  = ( D3DFORMAT )MAKESRGBFMT( D3DFMT_A8R8G8B8 );
	atgApp.m_d3dpp.PresentationInterval = D3DPRESENT_INTERVAL_IMMEDIATE;

	atgApp.Run();
}

HRESULT Sample::InitializeWithScreen()
{

	m_nXOffset = 0 ;
	m_nFontHeight = 16 ;


	m_pd3dDevice->Clear(0L, NULL, D3DCLEAR_TARGET, D3DCOLOR_XRGB(0,0,0), 1.0f, 0L);
	
	m_Font.Begin();
	m_Font.DrawText(m_nXOffset+  32, 16*2, 0xffffffff, L"Reading roms directory...please wait." ) ;
	m_Font.End();
	m_pd3dDevice->Present( NULL, NULL, NULL, NULL );


	if ( m_nFontHeight < 1 )
		m_nFontHeight = 16 ;


	m_namesPerPage =  (20*14 ) / ( m_nFontHeight+2) ;

	curr = 0 ;
	topIdx = 0 ;
	numfiles = 0 ;

	
	FindAvailRoms() ;

	if( FAILED(D3DXCreateTextureFromFile(m_pd3dDevice,"game:\\media\\background.jpg",&newtex)))
	{
			Skins = 0;
		}else{
			Skins = 1;
	}
	

	if(splashscreen)
	{
			m_pd3dDevice->SetTexture( 0, NULL );
			splashscreen->Release();
	}




	D3DXCreateTextureFromFile(m_pd3dDevice,"game:\\media\\pointer.png",&mouseptr);

	m_state = MAIN_MENU ;




   return S_OK;
}


void Sample::MoveCursor()
{
	char c ;
	int  lcv ;

		 ATG::Input::GetMergedInput();

		 if ( ATG::Input::m_Gamepads[0].wPressedButtons & XINPUT_GAMEPAD_DPAD_LEFT ) 
		{
			if ( topIdx >= m_namesPerPage )
			{
				topIdx -= m_namesPerPage ;
				curr = topIdx ;
			}
			else
			{
				topIdx = 0 ;
				curr = 0 ;
			}
		}
		else if ( ATG::Input::m_Gamepads[0].wPressedButtons & XINPUT_GAMEPAD_DPAD_RIGHT ) 
		{
			if ( topIdx + m_namesPerPage < numfiles )
			{
				topIdx += m_namesPerPage ;
				curr = topIdx ;
			}
			else
			{
				topIdx = numfiles - 1 ;
				curr = numfiles - 1 ;
			}
		}
		else if ( ATG::Input::m_Gamepads[0].wPressedButtons & XINPUT_GAMEPAD_DPAD_DOWN ) 
		{
			if ( curr == topIdx + (m_namesPerPage-1) )
			{
				if ( curr < numfiles - 1 )
				{
					topIdx++ ;
					curr++ ;
				}
			}
			else
			{
				if ( curr < numfiles-1 )
					curr++ ;
			}
		}
		else if ( ATG::Input::m_Gamepads[0].wPressedButtons & XINPUT_GAMEPAD_DPAD_UP ) 
		{
			if ( curr == topIdx )
			{
				if ( curr > 0 )
				{
					topIdx-- ;
					curr-- ;
				}
			}
			else
			{
				if ( curr > 0 )
					curr-- ;
			}
		}

}


typedef struct
{
	HANDLE eventHandle;
} YThread;

static volatile bool doterminate;
static volatile bool terminated;

DWORD WINAPI MainThread(LPVOID lpParameter)
{

    // Give this thread a name
    ATG::SetThreadName( GetCurrentThreadId(), "Main DS execute" );

	YThread *z=(YThread *)lpParameter;
	StopThread = 0;

	for(;;)
	{
	    EnterCriticalSection(&DsExec);
		if(doterminate) break;
		{
			NDS_exec<false>();
		}
		LeaveCriticalSection(&DsExec);
	}
	terminated = true;

	return 0;

}


void CreateMainThread()
{
	YThread *z;
	 
	hMainThread = CreateThread( NULL, 0, MainThread,( VOID* )0, 0, NULL );

	XSetThreadProcessor( hMainThread, 2 );

}


HRESULT Sample::DSInit()
{
  HRESULT hr;
  struct my_config my_config;
  struct NDS_fw_config_data fw_config;

  /* default the firmware settings, they may get changed later */
  NDS_FillDefaultFirmwareConfigData( &fw_config);

  init_config( &my_config);

    /* use any language set on the command line */
  if ( my_config.firmware_language != -1)
  {
    fw_config.language = my_config.firmware_language;
  }

  NDS_Init();

  /* Create the dummy firmware */
  NDS_CreateDummyFirmware( &fw_config);

  if ( !my_config.disable_sound) {
    SPU_ChangeSoundCore(SNDCORE_SDL, 735 * 4);
  }

	NDS_3D_ChangeCore(my_config.engine_3d);

  	sprintf(rompath,"game:\\roms\\%s",files[curr].filename);
	
	sprintf(statepath,"game:\\state\\%s",files[curr].filename);
	char *pt = strchr(statepath, '.');
	if (pt) *pt = 0;
	strcat(statepath, ".dst");

  if (NDS_LoadROM( rompath, my_config.cflash_disk_image_file) < 0) {
    printf("error while loading %s\n", rompath);
  }

   romloaded = TRUE;

   execute = TRUE;

	 if( FAILED( m_pd3dDevice->CreateTexture(256, 384, 1,0,D3DFMT_LIN_A1B5G5R5, D3DUSAGE_CPU_CACHED_MEMORY, &m_pCurrentImage,0)))
		 return 2; 


   	doterminate = false;
	terminated = false;
	CreateMainThread();


	 m_state = IN_GAME;

	return S_OK;
}

VOID Sample::ScreenGrab(char *fileName)
{
	// write the entire surface to the requested file
	D3DXSaveTextureToFile(fileName,D3DXIFF_BMP,m_pCurrentImage,NULL);
}


void NDS_Pause()
{
	if (!paused)
	{
		emu_halt();
		paused = TRUE;
		SPU_Pause(1);
		while (!paused) {}
		printf("Emulation paused\n");
	}
}

void NDS_UnPause()
{
	if (romloaded && paused)
	{
		paused = FALSE;
		execute = TRUE;
		SPU_Pause(0);
		printf("Emulation unpaused\n");
	}
}

//--------------------------------------------------------------------------------------
// Name: Initialize
// Desc: This creates all device-dependent display objects.
//--------------------------------------------------------------------------------------
HRESULT Sample::Initialize()
{
    m_pCurrentImage = NULL;


	// Create the font
    if( FAILED( m_Font.Create( "game:\\Media\\Fonts\\Arial_16.xpr" ) ) )
        return ATGAPPERR_MEDIANOTFOUND;

    // Confine text drawing to the title safe area
    m_Font.SetWindow( ATG::GetTitleSafeArea() );

    // Initialize the simple shaders
    ATG::SimpleShaders::Initialize( NULL, NULL );

    if( m_pCurrentImage )
    {
        m_pd3dDevice->SetTexture( 0, NULL );
        m_pCurrentImage->Release();

    }

	InitializeCriticalSection( &DsExec );

	InitializeWithScreen();
	
	return S_OK;
}

static signed long screen_to_touch_range_x( signed long scr_x, float size_ratio) {
  signed long touch_x = (signed long)((float)scr_x / 2.5f);
 // printf("Srcx %d , Cursor360x %d , DS cursorx %d\n",scr_x,touch_x,mouse.x);
  return touch_x;
}

static signed long screen_to_touch_range_y( signed long scr_y, float size_ratio)
{
  signed long touch_y = (signed long)((float)scr_y /1.0f);
 // printf("Srcy %d , Cursor360y %d , DS cursory %d\n",scr_y,touch_y,mouse.y);
  return touch_y;
}

/* Set mouse coordinates */
void set_mouse_coord(signed long x,signed long y)
{
  
  if(mouse.xx<0) 
	  mouse.xx = 0;
	  else 
   if(mouse.xx>640) 
	   mouse.xx = 640;
  if(mouse.yy<240) 
	  mouse.yy = 240;
		else 
  if(mouse.yy>480) 
	  mouse.yy = 480;
  
  if(x<0) x = 0; else if(x>255) x = 255;
  if(y<0) y = 0; else if(y>240) y = 240;
  mouse.x = x;
  mouse.y = y;


}

void Sample::DSPAD()
{
	    ATG::Input::GetMergedInput();
  
		if (ATG::Input::m_Gamepads[0].wButtons & XINPUT_GAMEPAD_B)
		{
		((u16 *)MMU.ARM9_REG)[0x130>>1] &= 0xFEFF;
		((u16 *)MMU.ARM7_REG)[0x130>>1] &= 0xFEFF;
		}else{
		((u16 *)MMU.ARM9_REG)[0x130>>1] |= 0x0100;
		((u16 *)MMU.ARM7_REG)[0x130>>1] |= 0x0100;
		}

		if (ATG::Input::m_Gamepads[0].wButtons & XINPUT_GAMEPAD_A)
		{
		((u16 *)MMU.ARM9_REG)[0x130>>1] &= 0xFDFF;
		((u16 *)MMU.ARM7_REG)[0x130>>1] &= 0xFDFF;
		}else{
		((u16 *)MMU.ARM9_REG)[0x130>>1] |= 0x0200;
		((u16 *)MMU.ARM7_REG)[0x130>>1] |= 0x0200;
		}

		if (ATG::Input::m_Gamepads[0].wButtons & XINPUT_GAMEPAD_BACK)
		{
		((u16 *)MMU.ARM9_REG)[0x130>>1] &= 0xFBFF;
		((u16 *)MMU.ARM7_REG)[0x130>>1] &= 0xFBFF;
		}else{
		((u16 *)MMU.ARM9_REG)[0x130>>1] |= 0x0400;
		((u16 *)MMU.ARM7_REG)[0x130>>1] |= 0x0400;
		}

		if (ATG::Input::m_Gamepads[0].wButtons & XINPUT_GAMEPAD_START)
		{
			((u16 *)MMU.ARM9_REG)[0x130>>1] &= 0xF7FF;
			((u16 *)MMU.ARM7_REG)[0x130>>1] &= 0xF7FF;
		}else{
			((u16 *)MMU.ARM9_REG)[0x130>>1] |= 0x0800;
			((u16 *)MMU.ARM7_REG)[0x130>>1] |= 0x0800;
		}

		if (ATG::Input::m_Gamepads[0].wButtons & XINPUT_GAMEPAD_DPAD_DOWN)
		{
		((u16 *)MMU.ARM9_REG)[0x130>>1] &= 0x7FFF;
		((u16 *)MMU.ARM7_REG)[0x130>>1] &= 0x7FFF;
		}else{
		((u16 *)MMU.ARM9_REG)[0x130>>1] |= 0x8000;
		((u16 *)MMU.ARM7_REG)[0x130>>1] |= 0x8000;
		}

		if (ATG::Input::m_Gamepads[0].wButtons & XINPUT_GAMEPAD_DPAD_UP)
		{
		((u16 *)MMU.ARM9_REG)[0x130>>1] &= 0xBFFF;
		((u16 *)MMU.ARM7_REG)[0x130>>1] &= 0xBFFF;
		}else{
		((u16 *)MMU.ARM9_REG)[0x130>>1] |= 0x4000;
		((u16 *)MMU.ARM7_REG)[0x130>>1] |= 0x4000;
		}

		if (ATG::Input::m_Gamepads[0].wButtons & XINPUT_GAMEPAD_DPAD_RIGHT)
		{
		((u16 *)MMU.ARM9_REG)[0x130>>1] &= 0xEFFF;
		((u16 *)MMU.ARM7_REG)[0x130>>1] &= 0xEFFF;
		}else{
		((u16 *)MMU.ARM9_REG)[0x130>>1] |= 0x1000;
		((u16 *)MMU.ARM7_REG)[0x130>>1] |= 0x1000;
		}

		if (ATG::Input::m_Gamepads[0].wButtons & XINPUT_GAMEPAD_DPAD_LEFT)
		{
			((u16 *)MMU.ARM9_REG)[0x130>>1] &= 0xDFFF;
			((u16 *)MMU.ARM7_REG)[0x130>>1] &= 0xDFFF;
		}else{
			((u16 *)MMU.ARM9_REG)[0x130>>1] |= 0x2000;
			((u16 *)MMU.ARM7_REG)[0x130>>1] |= 0x2000;
		}

		if (ATG::Input::m_Gamepads[0].wButtons & XINPUT_GAMEPAD_RIGHT_SHOULDER)
		{
			((u16 *)MMU.ARM9_REG)[0x130>>1] &= 0xFFFE;
			((u16 *)MMU.ARM7_REG)[0x130>>1] &= 0xFFFE;
		}else{
			((u16 *)MMU.ARM9_REG)[0x130>>1] |= 0x0001;
			((u16 *)MMU.ARM7_REG)[0x130>>1] |= 0x0001;
		}

		if (ATG::Input::m_Gamepads[0].wButtons & XINPUT_GAMEPAD_LEFT_SHOULDER)
		{
			((u16 *)MMU.ARM9_REG)[0x130>>1] &= 0xFFFD;
			((u16 *)MMU.ARM7_REG)[0x130>>1] &= 0xFFFD;
		}else{
			((u16 *)MMU.ARM9_REG)[0x130>>1] |= 0x0002;
			((u16 *)MMU.ARM7_REG)[0x130>>1] |= 0x0002;
		}

		if (ATG::Input::m_Gamepads[0].wButtons & XINPUT_GAMEPAD_Y)
		{
			((u16 *)MMU.ARM7_REG)[0x136>>1] &= 0xFEFF;
		}else{
			((u16 *)MMU.ARM7_REG)[0x136>>1] |= 0x0100;
		}

		if (ATG::Input::m_Gamepads[0].wButtons & XINPUT_GAMEPAD_X)
		{
			((u16 *)MMU.ARM7_REG)[0x136>>1] &= 0xFDFF;
		}else{
			((u16 *)MMU.ARM7_REG)[0x136>>1] |= 0x0200;
		}


	  if (ATG::Input::m_Gamepads[0].bLeftTrigger)
	  {
				NDS_Pause();
				savestate_load(statepath);
				NDS_UnPause();	  
	  }

	  if (ATG::Input::m_Gamepads[0].bRightTrigger)
	  {
				NDS_Pause();
				savestate_save(statepath);
				NDS_UnPause();
	  }


	  if (ATG::Input::m_Gamepads[0].wButtons & XINPUT_GAMEPAD_LEFT_THUMB) {
	  
	  		mouse.click = TRUE;
			mouse.down = FALSE;
	  }
	  else
	  {
		  mouse.down = TRUE;

		//  printf("%d\n",ATG::Input::m_Gamepads[0].sThumbLX);
		
		  if (ATG::Input::m_Gamepads[0].sThumbLX < -20000){
			--mouse.x;
			--mouse.xx;
		  } 
		
		  if (ATG::Input::m_Gamepads[0].sThumbLX >  20000){
			++mouse.x;
			++mouse.xx;
		  } 
		  
		  if (ATG::Input::m_Gamepads[0].sThumbLY <  -20000) {
			++mouse.y;
			++mouse.yy;
		  } 
		
		  if (ATG::Input::m_Gamepads[0].sThumbLY >  20000){
			--mouse.y;
			--mouse.yy;
		  }

          signed long scaled_x = screen_to_touch_range_x( mouse.xx,nds_screen_size_ratio);
          signed long scaled_y = screen_to_touch_range_y( mouse.y, nds_screen_size_ratio);

		  set_mouse_coord(scaled_x,  scaled_y );
	  }

}

void Sample::ExecEmu()
{

   if(mouse.down) NDS_setTouchPos(mouse.x, mouse.y);
    if(mouse.click)
      { 
        NDS_releaseTouch();
        mouse.click = FALSE;
      }

    DSPAD(); 
}



void OnScreenDebugMessage( const WCHAR* string, ...)
{

    va_list pArgList;
    va_start( pArgList, string );

    wvsprintfW( ExtFPS,string, pArgList );
    wcscat_s( ExtFPS, L"\n" );
	va_end( pArgList );
}

//--------------------------------------------------------------------------------------
// Name: Update
// Desc: Called once per frame, the call is the entry point for animating
//       the scene.
//--------------------------------------------------------------------------------------
HRESULT Sample::Update()
{
	if(m_state == IN_GAME)
	{
	
	ATG::Input::GetMergedInput();

	if ( (ATG::Input::m_Gamepads[0].wButtons & XINPUT_GAMEPAD_RIGHT_THUMB))
		{
		doterminate = true;
		while(!terminated) {
			Sleep(1);
		}
			CloseHandle(hMainThread);
			NDS_DeInit();
			m_pd3dDevice->Clear(0L, NULL, D3DCLEAR_TARGET, D3DCOLOR_XRGB(0,0,0), 1.0f, 0L);
			m_Font.Begin();
			m_Font.DrawText(m_nXOffset+  32, 16*2, 0xffffffff, L"Reloading game list..." ) ;
			m_Font.End();
			m_pd3dDevice->Present( NULL, NULL, NULL, NULL );

			if ( numfiles > 0 )
			{
				delete [] files;
				files = NULL;
			}
			
			numfiles = 0 ;
			InitializeWithScreen();
		}
	}


	if ( m_state == MAIN_MENU )
	{
			MoveCursor();
			ATG::Input::GetMergedInput();

			if(ATG::Input::m_Gamepads[0].wButtons & XINPUT_GAMEPAD_A)
			{
			
				if(newtex)
				{
					m_pd3dDevice->SetTexture( 0, NULL );
					newtex->Release();
				}

				m_Font.Begin();
				m_pd3dDevice->Clear(0L, NULL, D3DCLEAR_TARGET, D3DCOLOR_XRGB(0,0,0), 1.0f, 0L);
				m_Font.DrawText(m_nXOffset+  32, 16*2, 0xffffffff, L"Loading game...please wait." ) ;
				m_Font.End();
				m_pd3dDevice->Present( NULL, NULL, NULL, NULL );
				DSInit() ;
				m_state = IN_GAME ;
				return S_OK ;
			}

}
    return S_OK;
}

//--------------------------------------------------------------------------------------
// Name: Render
// Desc: Sets up render states, clears the viewport, and renders the scene.
//--------------------------------------------------------------------------------------
HRESULT Sample::Render()
{

		m_pd3dDevice->Clear(0L, NULL, D3DCLEAR_TARGET, D3DCOLOR_XRGB(0,0,0), 1.0f, 0L);

		D3DRECT d3drect =
		{
        20,                          // x1
        20,                          // y1
		Sample::m_d3dpp.BackBufferWidth-20,    // x2
        Sample::m_d3dpp.BackBufferHeight-20,   // y2
		};

		if ( m_state == INIT_SYSTEM )
		{
			InitializeWithScreen() ;
		}
		else if ( m_state == MAIN_MENU )
		{
			if(Skins){
			ATG::DebugDraw::DrawScreenSpaceTexturedRect(d3drect, newtex);
			}
			WCHAR str[2000];
			swprintf( str, L"Desmume 360 By Yoshihiro  %u games Found", numfiles);

			m_Font.Begin(); 
			m_Font.DrawText(m_nXOffset+ 10, 0, 0xffffffff, str );

		
			float fWinX = 10, fWinY = 40;
				for ( unsigned int idx = topIdx ; ( idx < numfiles ) && ( idx < topIdx+m_namesPerPage) ; idx++ )
			{
				if ( curr == idx )
				{
						m_Font.DrawText(m_nXOffset+fWinX  , fWinY, INFO_COLOR, files[idx].name );
				}
				else
				{
						m_Font.DrawText(m_nXOffset+fWinX  , fWinY, 0xffffffff, files[idx].name );
				}
				fWinY += (m_nFontHeight + 3);
			}
	
			
			// end font drawing
			m_Font.End();

			m_pd3dDevice->Present( NULL, NULL, NULL, NULL );
		}
		else if ( m_state == IN_GAME )
		{

		ExecEmu();
 		
		D3DLOCKED_RECT lockedRect;
		m_pCurrentImage->LockRect(0,&lockedRect,0,0);
		
		BYTE *sourceTemp=(BYTE*)GPU_screen;
		BYTE *destTemp=(BYTE*)lockedRect.pBits;

	  for(int i=0; i < 512*384; i++)
      {
		destTemp[i] = sourceTemp[i]; 
	  }

		m_pCurrentImage->UnlockRect(0);

		ATG::DebugDraw::DrawScreenSpaceTexturedRect( d3drect, m_pCurrentImage);

        D3DRECT CursorRect =
        {
                    ( LONG )( LONG )( mouse.xx ),   // x1
                    ( LONG )( LONG )( mouse.yy ),   // y1
                    ( LONG )( LONG )( mouse.xx + 32 ),   // x2
                    ( LONG )( LONG )( mouse.yy + 32 ),   // y2
        };

		 ATG::DebugDraw::DrawScreenSpaceTexturedRect( CursorRect, mouseptr );
		 
		m_pd3dDevice->Present( NULL, NULL, NULL, NULL );
  }

    return S_OK;
}

void Sample::FindAvailRoms()
{
	HANDLE hFind;	
	WIN32_FIND_DATA oFindData;

	files = new FILENAME[2000];

	hFind = FindFirstFile( "game:\\roms\\*", &oFindData);
	
	if (hFind == INVALID_HANDLE_VALUE)
	{
		return;
	}
	
	do 
	{

		//files[numfiles].name[0] = 0 ;
		char name[256];
		files[numfiles].name[0] = 0 ;
		sprintf(name,"%s",oFindData.cFileName);
		if(stricmp(strrchr(name, '.'), ".nds") == 0 ){
		strcpy( (char*)files[numfiles].filename,(char*)oFindData.cFileName ) ;
		strlwr( (char*)( files[numfiles].filename )) ;
        swprintf( files[numfiles].name, L"%S", files[numfiles].filename );
		numfiles++ ;
		}
	} while (FindNextFile(hFind, &oFindData));

	FindClose( hFind );
	QuickSort( 0, numfiles-1 ) ;
}


void Sample::QuickSort( int lo, int hi )
{
	int i = lo;
	int j = hi;
	char partitionStr[200];
	FILENAME tmpf;

	strcpy( partitionStr, (const char*)files[(i+j)/2].filename ) ;
	do
	{
		while ( strcmp( (const char*)files[i].filename, partitionStr ) < 0 ) i++ ;
		while ( strcmp( partitionStr, (const char*)files[j].filename ) < 0 ) j-- ;

		if (i <= j)
		{
			XMemCpy( &tmpf, &(files[i]), sizeof(tmpf) ) ;
			XMemCpy( &(files[i]), &(files[j]), sizeof(tmpf) ) ;
			XMemCpy( &(files[j]), &tmpf, sizeof(tmpf) ) ;
			i++;
			j--;
		}
	} while (i <= j);

	if (lo < j) QuickSort(lo, j);
	if (i < hi) QuickSort(i, hi);

	
}

