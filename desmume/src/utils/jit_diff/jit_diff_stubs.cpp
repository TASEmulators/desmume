/*
	Front-end-owned symbols that the emulator core references but that the
	differential harness has no use for. Everything here is inert: the harness
	executes single instructions, so there is no window, no audio device, no
	microphone and no INI file.

	Deliberately NOT stubbed: anything that carries emulation semantics. If a
	symbol can be satisfied by compiling a real source file, compile it instead
	(see the $extraSources list in tools/jit_matrix/build_core.ps1). A stub that
	returns a plausible value would quietly make the harness disagree with the
	emulator, which is exactly what this harness exists to detect.
*/

#include "types.h"

#ifdef HAVE_JIT

#include <windows.h>
#include <string>

#include "NDSSystem.h"
#include "mic.h"
#include "render3D.h"
#include "SPU.h"
#include "winutil.h"

// --- front-end execution flags ---------------------------------------------
volatile bool execute = true;
volatile bool paused = false;
int emu_paused = 0;

// Only used by the disassembly-viewer debug hook, never reached here.
HWND DisViewWnd[2] = { NULL, NULL };

// --- INI plumbing (PathInfo::ReadPathSettings) -----------------------------
// The harness never reads or writes settings, so an empty name is enough.
char IniName[MAX_PATH] = { 0 };
wchar_t IniNameW[MAX_PATH] = { 0 };
std::string _hack_alternateModulePathUtf8;

void WritePrivateProfileInt(char* /*appname*/, char* /*keyname*/, int /*val*/, char* /*file*/)
{
}

bool GetPrivateProfileBool(const char* /*appname*/, const char* /*keyname*/, bool defval, const char* /*filename*/)
{
	return defval;
}

// --- microphone -----------------------------------------------------------
// The Windows backend (frontend/windows/mic-win.cpp) is waveIn-based; the
// portable one is compiled out on WIN32. Neither is reachable from a
// single-instruction comparison, so these are no-ops.
// Mic_Init returns TRUE on success (BOOL, not errno): NDS_Init() aborts when a
// device fails to come up, so a no-op device still has to report success.
int Mic_Init(void) { return 1; }
void Mic_Reset(void) {}
void Mic_DeInit(void) {}
u8 Mic_ReadSample(void) { return 0x80; }

void mic_savestate(EMUFILE& /*f*/) {}
bool mic_loadstate(EMUFILE& /*f*/, int /*version*/) { return true; }

// --- renderer / sound core registries --------------------------------------
// Populated by the GUI front end at start-up. The harness never switches cores,
// so a one-entry NULL-terminated list keeps the lookup loops happy.
GPU3DInterface* core3DList[] = { NULL };
SoundInterface_struct* SNDCoreList[] = { NULL };

// --- zlib crc32 ------------------------------------------------------------
// The bundled zlib is compiled by the Windows project, not by the core archive,
// and zlib's own crc32.c is not in the tree layout we build from. Only
// NDS_LoadROM calls this and the harness never loads a ROM, but the standard
// implementation is used so the value is correct if that ever changes.
extern "C" unsigned long crc32(unsigned long crc, const unsigned char* buf, unsigned int len)
{
	static unsigned long table[256];
	static bool tableReady = false;
	if (!tableReady)
	{
		for (unsigned long i = 0; i < 256; i++)
		{
			unsigned long c = i;
			for (int k = 0; k < 8; k++)
				c = (c & 1) ? (0xEDB88320UL ^ (c >> 1)) : (c >> 1);
			table[i] = c;
		}
		tableReady = true;
	}

	unsigned long c = crc ^ 0xFFFFFFFFUL;
	for (unsigned int n = 0; n < len; n++)
		c = table[(c ^ buf[n]) & 0xFF] ^ (c >> 8);
	return c ^ 0xFFFFFFFFUL;
}

#endif // HAVE_JIT
