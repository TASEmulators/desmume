/*
	Link test for the AGG-free WebGPU OSD recorder (wgpu-agg.md 20.4).

	It instantiates WGPUOSDDrawTarget, installs it through AggDrawTarget* the way
	display.cpp does with Agg_setHudTarget(), draws every primitive the OSD and
	Lua use, and checks the CPU layer contract.  The point is the link line: it
	must succeed WITHOUT agg-2.5*.lib, which proves the recorder has no undefined
	agg::* symbols.

	Build from the repository root (needs the in-tree AGG headers on the include
	path, but never the library):

	  g++ -std=c++11 -Wno-deprecated-register \
	      -Idesmume/src/frontend/modules/osd/wgpu \
	      -Idesmume/src/frontend/modules/osd/agg \
	      -Idesmume/src/frontend/windows/agg/include \
	      -Idesmume/src/frontend/windows/agg/examples \
	      -Idesmume/src/frontend/windows/agg/font_freetype \
	      tools/osd_link_test/main.cpp \
	      desmume/src/frontend/modules/osd/wgpu/wgpu_osd_draw.cpp \
	      desmume/src/frontend/modules/osd/wgpu/wgpu_draw.cpp \
	      -o osd_link_test

	Exit code 0 means the recorder recorded, the layer is addressable through
	buf(), and the stride is width*4.
*/

#include "wgpu_osd_draw.h"

// Verify the recorder is usable through the AggDrawTarget interface, which is
// how display.cpp installs it with Agg_setHudTarget().
static void InstallTarget(AggDrawTarget **slot, WGPUOSDDrawTarget *target)
{
	*slot = target;
	(*slot)->clear();
	(*slot)->fillColor(0, 0, 255, 255);
	(*slot)->rectangle(1, 1, 20, 20);
	(*slot)->renderText(4, 4, "OSD");
}

int main()
{
	WGPUOSDDrawTarget target(256, 384);
	AggDrawTarget *hud = NULL;
	InstallTarget(&hud, &target);

	target.clear();
	target.fillColor(255, 0, 0, 128);
	target.rectangle(10, 10, 50, 50);
	target.roundedRect(2, 2, 30, 30, 1);
	target.ellipse(80, 80, 20, 12);
	target.lineColor(255, 255, 255);
	target.line(0, 0, 100, 100);
	target.arc(60, 60, 12, 12, 0.0, 3.0);
	target.star(120, 120, 5, 12, 0.0, 5);
	target.curve(0, 0, 10, 20, 30, 0);
	target.triangle(1, 1, 10, 1, 5, 10);
	target.renderTextDropshadowed(20, 20, "ABC");

	uint32_t *pixels = target.pixelBuffer();
	if (pixels != NULL)
	{
		pixels[0] = 0x80FF0000u;
	}

	const bool hasContent = target.hasContent();
	const int stride = target.pixelStrideBytes();
	const bool installed = (hud == &target) && (hud->buf().buf() != NULL);
	target.clear();

	return (hasContent && installed && (stride == 256 * 4)) ? 0 : 1;
}
