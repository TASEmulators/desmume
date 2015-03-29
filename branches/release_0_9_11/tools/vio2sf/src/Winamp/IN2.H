
#ifndef NULLSOFT_WINAMP_IN2H
#define NULLSOFT_WINAMP_IN2H
#include "out.h"

// note: exported symbol is now winampGetInModule2.

#define IN_UNICODE 0x0F000000

#ifdef UNICODE_INPUT_PLUGIN
#define in_char wchar_t
#define IN_VER (IN_UNICODE | 0x100)
#else
#define in_char char
#define IN_VER 0x100
#endif

#define IN_MODULE_FLAG_USES_OUTPUT_PLUGIN 1
// By default, Winamp assumes that your input plugin wants to use Winamp's EQ, and doesn't do replay gain
// if you handle any of these yourself (EQ, Replay Gain adjustments), then set these flags accordingly
#define IN_MODULE_FLAG_EQ 2 // set this if you do your own EQ
#define IN_MODULE_FLAG_REPLAYGAIN 8 // set this if you adjusted volume for replay gain
																		// for tracks with no replay gain metadata, you should clear this flag 
																		// UNLESS you handle "non_replaygain" gain adjustment yourself
#define IN_MODULE_FLAG_REPLAYGAIN_PREAMP 16 // use this if you queried for the replay gain preamp parameter and used it 
																						// this parameter is new to 5.54
typedef struct 
{
	int version;				// module type (IN_VER)
	char *description;			// description of module, with version string

	HWND hMainWindow;			// winamp's main window (filled in by winamp)
	HINSTANCE hDllInstance;		// DLL instance handle (Also filled in by winamp)

	char *FileExtensions;		// "mp3\0Layer 3 MPEG\0mp2\0Layer 2 MPEG\0mpg\0Layer 1 MPEG\0"
								// May be altered from Config, so the user can select what they want
	
	int is_seekable;			// is this stream seekable? 
	int UsesOutputPlug;			// does this plug-in use the output plug-ins? (musn't ever change, ever :)
													// note that this has turned into a "flags" field
													// see IN_MODULE_FLAG_*

	void (*Config)(HWND hwndParent); // configuration dialog
	void (*About)(HWND hwndParent);  // about dialog

	void (*Init)();				// called at program init
	void (*Quit)();				// called at program quit

#define GETFILEINFO_TITLE_LENGTH 2048 
	void (*GetFileInfo)(const in_char *file, in_char *title, int *length_in_ms); // if file == NULL, current playing is used

#define INFOBOX_EDITED 0
#define INFOBOX_UNCHANGED 1
	int (*InfoBox)(const in_char *file, HWND hwndParent);
	
	int (*IsOurFile)(const in_char *fn);	// called before extension checks, to allow detection of mms://, etc
	// playback stuff
	int (*Play)(const in_char *fn);		// return zero on success, -1 on file-not-found, some other value on other (stopping winamp) error
	void (*Pause)();			// pause stream
	void (*UnPause)();			// unpause stream
	int (*IsPaused)();			// ispaused? return 1 if paused, 0 if not
	void (*Stop)();				// stop (unload) stream

	// time stuff
	int (*GetLength)();			// get length in ms
	int (*GetOutputTime)();		// returns current output time in ms. (usually returns outMod->GetOutputTime()
	void (*SetOutputTime)(int time_in_ms);	// seeks to point in stream (in ms). Usually you signal your thread to seek, which seeks and calls outMod->Flush()..

	// volume stuff
	void (*SetVolume)(int volume);	// from 0 to 255.. usually just call outMod->SetVolume
	void (*SetPan)(int pan);	// from -127 to 127.. usually just call outMod->SetPan
	
	// in-window builtin vis stuff

	void (*SAVSAInit)(int maxlatency_in_ms, int srate);		// call once in Play(). maxlatency_in_ms should be the value returned from outMod->Open()
	// call after opening audio device with max latency in ms and samplerate
	void (*SAVSADeInit)();	// call in Stop()


	// simple vis supplying mode
	void (*SAAddPCMData)(void *PCMData, int nch, int bps, int timestamp); 
											// sets the spec data directly from PCM data
											// quick and easy way to get vis working :)
											// needs at least 576 samples :)

	// advanced vis supplying mode, only use if you're cool. Use SAAddPCMData for most stuff.
	int (*SAGetMode)();		// gets csa (the current type (4=ws,2=osc,1=spec))
							// use when calling SAAdd()
	int (*SAAdd)(void *data, int timestamp, int csa); // sets the spec data, filled in by winamp


	// vis stuff (plug-in)
	// simple vis supplying mode
	void (*VSAAddPCMData)(void *PCMData, int nch, int bps, int timestamp); // sets the vis data directly from PCM data
											// quick and easy way to get vis working :)
											// needs at least 576 samples :)

	// advanced vis supplying mode, only use if you're cool. Use VSAAddPCMData for most stuff.
	int (*VSAGetMode)(int *specNch, int *waveNch); // use to figure out what to give to VSAAdd
	int (*VSAAdd)(void *data, int timestamp); // filled in by winamp, called by plug-in


	// call this in Play() to tell the vis plug-ins the current output params. 
	void (*VSASetInfo)(int srate, int nch); // <-- Correct (benski, dec 2005).. old declaration had the params backwards


	// dsp plug-in processing: 
	// (filled in by winamp, calld by input plug)

	// returns 1 if active (which means that the number of samples returned by dsp_dosamples
	// could be greater than went in.. Use it to estimate if you'll have enough room in the
	// output buffer
	int (*dsp_isactive)(); 

	// returns number of samples to output. This can be as much as twice numsamples. 
	// be sure to allocate enough buffer for samples, then.
	int (*dsp_dosamples)(short int *samples, int numsamples, int bps, int nch, int srate);


	// eq stuff
	void (*EQSet)(int on, char data[10], int preamp); // 0-64 each, 31 is +0, 0 is +12, 63 is -12. Do nothing to ignore.

	// info setting (filled in by winamp)
	void (*SetInfo)(int bitrate, int srate, int stereo, int synched); // if -1, changes ignored? :)

	Out_Module *outMod; // filled in by winamp, optionally used :)
} In_Module;

// return values from the winampUninstallPlugin(HINSTANCE hdll, HWND parent, int param)
// which determine if we can uninstall the plugin immediately or on winamp restart
//
// uninstall support was added from 5.0+ and uninstall now support from 5.5+
// it is down to you to ensure that if uninstall now is returned that it will not cause a crash
// (ie don't use if you've been subclassing the main window)
#define IN_PLUGIN_UNINSTALL_NOW    0x1
#define IN_PLUGIN_UNINSTALL_REBOOT 0x0

#endif