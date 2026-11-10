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

#include "../agg/aggdraw.h"

// ---------------------------------------------------------------------------
// Stage 2 of the WebGPU OSD migration: draw the primitives agg_osd.cpp uses
// with signed-distance-field shaders instead of AGG's CPU rasterizer.
//
// Stage 3 adds bitmap text: the AGG raster fonts are packed into an R8 coverage
// atlas and each character becomes one quad, so the normal HUD text no longer
// touches the CPU buffer. Vector fonts, fallback primitives and anything else
// outside the stage-2/3 scope still go through AGG into the target's CPU
// buffer, which is composited on top at render() time.
//
// WGPUOSDAggTarget is an AggDrawTarget, so the existing OSD code keeps calling
// fillColor()/rectangle()/renderText() exactly as before.
//
// WGPUOSDRenderer owns the pipelines and the target texture. It never creates a
// device: a frontend hands it the device/queue it already has, and the result
// is a premultiplied BGRA8 texture that the stage-1 overlay path can sample.
// Everything is exposed through void* so webgpu.h stays out of this header.
//
// Known stage-2/3 limitations, all outside what agg_osd.cpp currently does:
//   - linear gradients only, with the default profile
//   - no affine transformations (non-identity transforms fall back to AGG)
//   - no blend modes other than BlendAlpha (they fall back to AGG)
//   - clip boxes are not applied to recorded shapes or text
//   - fallback primitives are rasterized by AGG into the text layer, so they
//     are always composited after every recorded shape and glyph.
//     agg_osd.cpp has no fallback primitives; a caller that mixes them must
//     keep the z-order in mind.
//   - vector fonts stay on AGG (AGG2D_USE_VECTORFONTS is off in this build)
//   - bitmap text needs setFonts() to be registered; unregistered fonts fall
//     back to AGG. Text uses the current lineColor and ignores masterAlpha,
//     exactly like Agg2D::renderText.
//   - with partial-alpha text colors the GPU writes correct premultiplied
//     pixels, while AGG's bitmap-text span writes straight color and leaves a
//     1/255 alpha haze around glyphs. OSD text is always opaque, so this does
//     not change the current HUD; it is an intentional fix of an AGG quirk.
//   - Lua's gui.* drawing uses primitives, transforms and clips beyond this
//     scope; keep it on the AGG target until it is migrated separately.
// ---------------------------------------------------------------------------

// Shapes recorded by WGPUOSDAggTarget. `type` is one of WGPUOSDShapeType.
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

class WGPUOSDAggTarget : public T_AGG_RGBA
{
public:
	WGPUOSDAggTarget(int width, int height);
	~WGPUOSDAggTarget();

	bool beginGPU(void *instance, void *device, void *queue);
	bool setFonts(const WGPUOSDFont *fonts, int count);
	bool render();
	bool readPixels(uint8_t *dst, int dstStrideBytes);
	void *textureView() const;

	int targetWidth() const;
	int targetHeight() const;

	// AggDrawTarget
	void clear();
	void setDrawTargetDims(agg::int8u *buf, int width, int height, int stride);

	void fillColor(AggColor c);
	void fillColor(unsigned r, unsigned g, unsigned b, unsigned a = 255);
	void noFill();

	void lineColor(AggColor c);
	void lineColor(unsigned r, unsigned g, unsigned b, unsigned a = 255);
	void noLine();

	void lineWidth(double w);

	void fillLinearGradient(double x1, double y1, double x2, double y2, AggColor c1, AggColor c2, double profile = 1.0);
	void lineLinearGradient(double x1, double y1, double x2, double y2, AggColor c1, AggColor c2, double profile = 1.0);

	void masterAlpha(double a);

	// Bitmap text: recorded on the GPU when the active font is registered.
	// Vector fonts and non-recordable state fall back to AGG.
	void setFont(const std::string &name);
	void renderText(double dstX, double dstY, const std::string &str);

	void line(double x1, double y1, double x2, double y2);
	void triangle(double x1, double y1, double x2, double y2, double x3, double y3);
	void rectangle(double x1, double y1, double x2, double y2);
	void roundedRect(double x1, double y1, double x2, double y2, double r);
	void roundedRect(double x1, double y1, double x2, double y2, double rx, double ry);
	void roundedRect(double x1, double y1, double x2, double y2, double rxBottom, double ryBottom, double rxTop, double ryTop);
	void ellipse(double cx, double cy, double rx, double ry);
	void arc(double cx, double cy, double rx, double ry, double start, double sweep);
	void star(double cx, double cy, double r1, double r2, double startAngle, int numRays);
	void curve(double x1, double y1, double x2, double y2, double x3, double y3);
	void polygon(double *xy, int numPoints);

private:
	typedef T_AGG_RGBA BASE;

	static agg::int8u *AllocatePixels(int width, int height);

	WGPUOSDAggTarget(int width, int height, agg::int8u *pixels);

	bool CanRecordShapes();
	void RecordShape(uint32_t type, double x0, double y0, double x1, double y1, double radius);
	void RecordPolygon(const WGPUOSDPoint *points, int count);
	void RecordPolyline(const WGPUOSDPoint *points, int count);
	void RecordText(double dstX, double dstY, const std::string &str);
	uint32_t PackColor(AggColor c) const;

	agg::int8u *m_ownedPixels;
	agg::int8u *m_pixels;
	int m_width;
	int m_height;
	int m_stride;

	bool m_hasFill;
	bool m_hasStroke;
	bool m_fillGradient;
	bool m_lineGradient;
	AggColor m_fill0;
	AggColor m_fill1;
	AggColor m_line0;
	AggColor m_line1;
	double m_gradientX0, m_gradientY0, m_gradientX1, m_gradientY1;
	double m_lineGradientX0, m_lineGradientY0, m_lineGradientX1, m_lineGradientY1;
	double m_strokeWidth;
	double m_masterAlpha;

	const agg::int8u *m_fontData;

	std::vector<WGPUOSDShape> m_shapes;
	std::vector<WGPUOSDPoint> m_points;
	std::vector<WGPUOSDTextRun> m_textRuns;
	std::vector<char> m_textBytes;

	WGPUOSDRenderer m_renderer;

	WGPUOSDAggTarget(const WGPUOSDAggTarget &);
	WGPUOSDAggTarget &operator=(const WGPUOSDAggTarget &);
};

#endif // _WGPU_DRAW_H_
