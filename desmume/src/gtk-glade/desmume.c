#include "desmume.h"

#define EMULOOP_PRIO (G_PRIORITY_HIGH_IDLE + 20)
gboolean EmuLoop(gpointer data);
static BOOL regMainLoop = FALSE;


volatile BOOL execute = FALSE;
BOOL click = FALSE;
BOOL fini = FALSE;
unsigned long glock = 0;

void desmume_mem_init();

u8 *desmume_rom_data = NULL;
u32 desmume_last_cycle;

void desmume_init()
{
	NDS_Init();
        SPU_ChangeSoundCore(SNDCORE_SDL, 735 * 4);
	execute = FALSE;
}

void desmume_free()
{
	execute = FALSE;
	NDS_DeInit();
}

int desmume_open(const char *filename)
{
        int i = NDS_LoadROM(filename, MC_TYPE_AUTODETECT, 1);
	return i;
}

void desmume_pause()
{
	execute = FALSE;
}

void desmume_resume()
{

	execute = TRUE;
	if(!regMainLoop)
	{
		g_idle_add_full(EMULOOP_PRIO, &EmuLoop, NULL, NULL); regMainLoop = TRUE;
	}
}

void desmume_reset()
{
	NDS_Reset();
	desmume_resume();
}

void desmume_toggle()
{
	execute = (execute) ? FALSE : TRUE;
}
BOOL desmume_running()
{
	return execute;
}

void desmume_cycle()
{
	process_joy_events();
	desmume_last_cycle = NDS_exec((560190 << 1) - desmume_last_cycle, FALSE);
        SPU_Emulate();
}

void desmume_keypad(u16 k)
{
	unsigned short k_ext = (k >> 10) & 0x3;
	unsigned short k_pad = k & 0x3FF;
	((unsigned short *)ARM9Mem.ARM9_REG)[0x130>>1] = ~k_pad;
	((unsigned short *)MMU.ARM7_REG)[0x130>>1] = ~k_ext;
}


/////////////////////////////// TOOLS MANAGEMENT ///////////////////////////////
#if 0
//#include "dTool.h"

extern const dTool_t *dTools_list[];
extern const int dTools_list_size;

BOOL *dTools_running;

void Start_dTool(GtkWidget *widget, gpointer data)
{
	int tool = GPOINTER_TO_INT(data);
	
	if(dTools_running[tool]) return;
	
	dTools_list[tool]->open(tool);
	dTools_running[tool] = TRUE;
}

void dTool_CloseCallback(int tool)
{
	dTools_running[tool] = FALSE;
}

/////////////////////////////// MAIN EMULATOR LOOP ///////////////////////////////


static inline void _updateDTools()
{
	int i;
	for(i = 0; i < dTools_list_size; i++)
	{
		if(dTools_running[i]) { dTools_list[i]->update(); }
	}
}
#endif


Uint32 fps, fps_SecStart, fps_FrameCount;
static void Draw()
{
}
gboolean EmuLoop(gpointer data)
{
	int i;
	
	if(desmume_running())	/* Si on est en train d'executer le programme ... */
	{
		
		fps_FrameCount += Frameskip + 1;
		if(!fps_SecStart) fps_SecStart = SDL_GetTicks();
		if(SDL_GetTicks() - fps_SecStart >= 1000)
		{
			fps_SecStart = SDL_GetTicks();
			fps = fps_FrameCount;
			fps_FrameCount = 0;
			
			char Title[32];
			sprintf(Title, "Desmume - %dfps", fps);
			gtk_window_set_title(GTK_WINDOW(pWindow), Title);
		}
		
		desmume_cycle();	/* Emule ! */
		for(i = 0; i < Frameskip; i++) desmume_cycle();	/* cycles supplémentaires pour le frameskip */
		
		Draw();
		
	//	_updateDTools();
		gtk_widget_queue_draw(pDrawingArea);
		gtk_widget_queue_draw(pDrawingArea2);
		
		return TRUE;
	}
	
	regMainLoop = FALSE;
	return FALSE;
}

