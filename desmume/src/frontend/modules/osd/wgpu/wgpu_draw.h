/*
	Copyright (C) 2026 DeSmuME team

	This file is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 2 of the License, or
	(at your option) any later version.

	This file is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with the this software.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef _WGPU_DRAW_H_
#define _WGPU_DRAW_H_

#include <stdint.h>
#include <vector>

// This header is deliberately AGG-free.  It only describes the POD recording
// format and the renderer; the AggDrawTarget-compatible recorder lives in
// wgpu_osd_draw.h.  Keeping libagg out of this translation unit is what lets
// the Windows default build link without agg-2.5*.lib (see wgpu-agg.md 20).

// ---------------------------------------------------------------------------
// Stage 2 of the WebGPU OSD migration: draw the primitives agg_osd.cpp uses
// with signed-distance-field shaders instead of AGG's CPU rasterizer.
//
// Stage 3 adds bitmap text: the AGG raster font blobs are packed into an R8
// coverage atlas and each character becomes one quad, so the normal HUD text
// no longer touches the CPU buffer. The CPU layer now only carries Lua's
// gui.* pixels, which are composited on top at render() time.
//
// WGPUOSDDrawTarget (wgpu_osd_draw.h) implements the AggDrawTarget interface
// without instantiating Agg2D, so the existing OSD code keeps calling
// fillColor()/rectangle()/renderText() exactly as before.
//
// WGPUOSDRenderer owns the pipelines and the target texture. It never creates a
// device: a frontend hands it the device/queue it already has, and the result
// is a premultiplied BGRA8 texture that the stage-1 overlay path can sample.
// Everything is exposed through void* so webgpu.h stays out of this header.
//
// Known stage-2/3 limitations, all outside what agg_osd.cpp currently does:
//   - linear gradients only, with the default profile
//   - affine transformations, clip boxes and non-BlendAlpha blend modes are
//     ignored (the recorder has no CPU rasterizer to fall back to)
//   - the gui.* CPU layer is always composited after every shape and glyph,
//     so mixing agg.* and gui.* cannot preserve strict z-order
//   - bitmap text needs setFonts() to be registered; an unknown font keeps
//     the previous one. Text uses the current lineColor and ignores
//     masterAlpha, exactly like Agg2D::renderText.
//   - with partial-alpha text colors the GPU writes correct premultiplied
//     pixels (see wgpu-agg.md 13.3 for the AGG quirk this intentionally fixes)
// ---------------------------------------------------------------------------

// Shapes recorded by WGPUOSDDrawTarget. `type` is one of WGPUOSDShapeType.
enum WGPUOSDShapeType
{
	WGPUOSDShape_Rectangle = 0,
	WGPUOSDShape_RoundedRect = 1,
	WGPUOSDShape_Ellipse = 2,
	WGPUOSDShape_Polygon = 3,
	WGPUOSDShape_Line = 4,
	// An open path stroked with the same round-join distance as a polygon, but
	// without the closing edge and without a fill. Lua's agg.arc()/agg.curve()
	// record into this so no AGG fallback is needed.
	WGPUOSDShape_Polyline = 5
};

struct WGPUOSDPoint
{
	float x;
	float y;
};

// A bitmap font handed to WGPUOSDRenderer. `data` points at an AGG raster-font
// blob (fonts_list[].font). `size` may be 0 when the blob is trusted; the
// renderer then derives its length from the glyph offset table.
struct WGPUOSDFont
{
	const char *name;
	const uint8_t *data;
	uint32_t size;
};

// One recorded text run. `charOffset`/`charCount` index the character buffer
// passed alongside the runs to render(). Text is expanded to one instance per
// glyph inside the renderer, where the atlas layout is known.
struct WGPUOSDTextRun
{
	const uint8_t *font;
	double x;
	double y;
	uint32_t color; // 0xRRGGBBAA, straight (not premultiplied)
	uint32_t charOffset;
	uint32_t charCount;
};

// Colors are 0xRRGGBBAA, straight (not premultiplied), matching AggColor.
struct WGPUOSDShape
{
	uint32_t type;
	uint32_t hasFill;
	uint32_t hasStroke;
	uint32_t fillGradient;
	uint32_t lineGradient;

	double lineWidth;

	// Rectangle/RoundedRect: x0,y0,x1,y1 are the two corners.
	// Ellipse: x0,y0 is the center, x1,y1 the radii.
	// Line: the two endpoints.
	double x0, y0, x1, y1;

	double radius;

	double fillX0, fillY0, fillX1, fillY1;
	double lineX0, lineY0, lineX1, lineY1;

	uint32_t fill0;
	uint32_t fill1;
	uint32_t line0;
	uint32_t line1;

	uint32_t pointOffset;
	uint32_t pointCount;
};

class WGPUOSDRenderer
{
public:
	WGPUOSDRenderer();
	~WGPUOSDRenderer();

	// `instance` is only used for readPixels(); render() needs device and queue.
	// `device` and `queue` are WGPUDevice/WGPUQueue carried as void*.
	//
	// Fonts must be registered before begin() to be packed into the atlas at
	// initialization. Calling setFonts() after begin() rebuilds it.
	bool setFonts(const WGPUOSDFont *fonts, int count);
	// True only when the font is registered, the renderer is ready and the
	// font made it into the current atlas.
	bool hasFont(const void *data) const;
	bool begin(void *instance, void *device, void *queue, int width, int height);
	void kill();
	bool isReady() const;

	int width() const;
	int height() const;

	// Draws `shapes`, then `runs` (bitmap glyphs), then the CPU fallback layer
	// `textPixels`. `text` holds the characters referenced by the runs. Text
	// pixels are BGRA8888, top-down.
	bool render(const uint32_t *textPixels, int textStrideBytes,
	            const WGPUOSDShape *shapes, int shapeCount,
	            const WGPUOSDPoint *points, int pointCount,
	            const WGPUOSDTextRun *runs, int runCount,
	            const char *text, int textLength);

	// Test/screenshot helper: copies the target back as premultiplied BGRA8.
	// `dstStrideBytes` must be at least width() * 4.
	bool readPixels(uint8_t *dst, int dstStrideBytes);

	// A WGPUTextureView for the target, or NULL. Valid until the next begin()
	// or kill().
	void *textureView() const;

private:
	struct Impl;
	Impl *_impl;

	WGPUOSDRenderer(const WGPUOSDRenderer &);
	WGPUOSDRenderer &operator=(const WGPUOSDRenderer &);
};

// The canonical WGSL source of the shape pipeline. Exposed so tests (and a
// future build step) can verify that the embedded copy has not drifted from
// hud_shapes.wgsl.
const char *WGPUOSDShapeShaderSource();

// Same contract for the bitmap-text pipeline and hud_text.wgsl.
const char *WGPUOSDTextShaderSource();

// The AggDrawTarget-compatible recorder is declared in wgpu_osd_draw.h.
// It consumes the structs above and is the only piece that needs aggdraw.h.

#endif // _WGPU_DRAW_H_
