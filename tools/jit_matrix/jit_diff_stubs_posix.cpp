/*
	POSIX counterpart of desmume/src/utils/jit_diff/jit_diff_stubs.cpp.

	The Windows harness gets away with a single stub file because the core
	archive it links against is built by build_core.ps1, which compiles the
	emulator sources and nothing else. On a POSIX host the natural archive is
	the one meson already produced for the front end, and that archive carries
	front-end translation units too (path.cpp, mic_sdl.cpp ...). Those pull in
	glib and SDL, which the build script links; what they do not provide is the
	small set of globals the *real* front end is supposed to define. This file
	supplies exactly those, and nothing else -- anything that carries emulation
	semantics is compiled from the real source instead.

	Kept out of desmume/src/utils/jit_diff/ on purpose: build_diff.ps1 compiles
	every *.cpp in that directory, and jit_diff_stubs.cpp is Windows-only, so
	the two platforms cannot share that folder.
*/

#include "types.h"

#ifdef HAVE_JIT

#include "NDSSystem.h"
#include "render3D.h"
#include "SPU.h"

// Read by emu_halt()/NDS_Init() paths. The harness never runs a frame loop, so
// "running" is the honest value: a false here turns emu_halt() into a spin.
volatile bool execute = true;
volatile bool paused = false;
int emu_paused = 0;

// SPU_Init()/GPUSubsystem::Set3DRendererByID() look their core up *by id* in
// these tables. An empty list makes the lookup fail and NDS_Init() returns
// before it creates the wifi handler, which then turns every instruction that
// reaches emu_halt() into a null-pointer fault instead of a halt.
//
// Only the null 3D core is listed: gpu3DRasterize lives in the *Windows* front
// end (rasterize.h/cpp there), while the POSIX build ships its own software
// rasterizer object that this harness never calls. Id 0 -- the default -- is
// gpu3DNull, which is all NDS_Init() touches.
GPU3DInterface* core3DList[] = { &gpu3DNull, NULL };
SoundInterface_struct* SNDCoreList[] = { &SNDDummy, NULL };

#endif // HAVE_JIT
