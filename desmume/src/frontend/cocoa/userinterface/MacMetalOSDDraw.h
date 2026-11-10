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

#ifndef _MAC_METAL_OSD_DRAW_H_
#define _MAC_METAL_OSD_DRAW_H_

// ---------------------------------------------------------------------------
// Metal port of the WebGPU OSD backend (frontend/modules/osd/wgpu/wgpu_draw.*).
//
// The WGPU version's WGPUOSDAggTarget inherits T_AGG_RGBA, which cocoa does not
// compile (no HAVE_LIBAGG, no osd/agg).  This header therefore provides an
// AGG-free recorder with the same method names and the same recording semantics
// that agg_osd.cpp / lua-engine.cpp use.  It is plain C++ so that lua-engine.cpp
// (compiled as C++, not Objective-C++) can include it.
//
// The recorder only stores primitives; MacMetalOSDRenderer turns them into SDF
// instances, glyph quads and a CPU fallback texture on a 256x384 premultiplied
// BGRA8 offscreen target.  Coordinates are pixels with the origin at the top
// left of that target, exactly like the WGPU implementation.
//
// Limitations inherited from wgpu_draw.h:
//   - linear gradients only, default profile
//   - no affine transforms, no clip boxes, only BlendAlpha
//   - text uses the registered AGG raster fonts; unknown names fall back to the
//     default font
//   - the CPU fallback layer is always composited after shapes and glyphs
// ---------------------------------------------------------------------------

#include <stdint.h>
#include <string>
#include <vector>

#define METAL_OSD_WIDTH             256
#define METAL_OSD_HEIGHT            384
#define METAL_OSD_MAX_SHAPE_POINTS  64

// A registered AGG raster-font blob.  `size` may be 0 for trusted blobs.
struct MetalOSDFont
{
	const char *name;
	const uint8_t *data;
	uint32_t size;
};

// Generated from frontend/windows/osd_fonts/fonts/*.bin.  Names and order match
// fonts_list[] in frontend/modules/osd/agg/aggdraw.cpp, so old INI font names
// and Lua scripts keep working.
extern const MetalOSDFont MacMetalOSDEmbeddedFonts[];
extern const int MacMetalOSDEmbeddedFontCount;

enum MetalOSDShapeType
{
	MetalOSDShape_Rectangle   = 0,
	MetalOSDShape_RoundedRect = 1,
	MetalOSDShape_Ellipse     = 2,
	MetalOSDShape_Polygon     = 3,
	MetalOSDShape_Line        = 4,
	MetalOSDShape_Polyline    = 5  // open path, no closing edge and no fill
};

struct MetalOSDPoint
{
	float x;
	float y;
};

// Colors are 0xRRGGBBAA, straight (not premultiplied), matching AggColor.
struct MetalOSDTextRun
{
	const uint8_t *font;
	double x;
	double y;
	uint32_t color;
	uint32_t charOffset;
	uint32_t charCount;
};

struct MetalOSDShape
{
	uint32_t type;
	uint32_t hasFill;
	uint32_t hasStroke;
	uint32_t fillGradient;
	uint32_t lineGradient;

	double lineWidth;

	// Rectangle/RoundedRect: two corners. Ellipse: center + radii.
	// Line: two endpoints.
	double x0, y0, x1, y1;
	double radius;

	double fillX0, fillY0, fillX1, fillY1;
	double lineX0, lineY0, lineX1, lineY1;

	uint32_t fill0, fill1, line0, line1;

	uint32_t pointOffset;
	uint32_t pointCount;
};

// One published OSD frame.  Everything in here is a value type, so the emu
// thread can publish and the render thread can consume without sharing storage.
struct MetalOSDFrame
{
	std::vector<MetalOSDShape>   shapes;
	std::vector<MetalOSDPoint>   points;
	std::vector<MetalOSDTextRun> runs;
	std::vector<char>            text;
	std::vector<uint32_t>        cpuPixels;   // 256x384 premultiplied BGRA8
	bool                         hasCpuLayer;
	uint64_t                     generation;

	MetalOSDFrame() : hasCpuLayer(false), generation(0) {}

	bool hasContent() const
	{
		return !shapes.empty() || !runs.empty() || hasCpuLayer;
	}
};

struct MetalOSDColor
{
	unsigned r, g, b, a;
	MetalOSDColor(unsigned r_ = 255, unsigned g_ = 255, unsigned b_ = 255, unsigned a_ = 255)
		: r(r_), g(g_), b(b_), a(a_) {}
};

// AGG-free equivalent of WGPUOSDAggTarget.  Method names intentionally match
// the AggDrawTarget subset used by agg_osd.cpp and Lua's agg.* table.
class MacMetalOSDDrawTarget
{
public:
	MacMetalOSDDrawTarget();

	void clear();

	// Direct pixel access for the CPU fallback layer (premultiplied BGRA8).
	// Calling this allocates the layer and marks it dirty for the whole frame.
	uint32_t *pixelBuffer();
	int pixelStrideBytes() const { return METAL_OSD_WIDTH * 4; }
	void markPixelsDirty() { m_frame.hasCpuLayer = !m_frame.cpuPixels.empty(); }

	const MetalOSDFrame &frame() const { return m_frame; }
	MetalOSDFrame &frame() { return m_frame; }

	// State
	void fillColor(MetalOSDColor c);
	void fillColor(unsigned r, unsigned g, unsigned b, unsigned a = 255);
	void lineColor(MetalOSDColor c);
	void lineColor(unsigned r, unsigned g, unsigned b, unsigned a = 255);
	MetalOSDColor fillColor() const { return m_fill0; }
	MetalOSDColor lineColor() const { return m_line0; }
	void noFill();
	void noLine();
	// Needed to save/restore the paint state around gui.osdtext(), which has to
	// borrow lineColor() (AggDrawTarget's text color) for one run.
	bool hasFill() const { return m_hasFill; }
	bool hasStroke() const { return m_hasStroke; }
	void setHasFill(bool enabled) { m_hasFill = enabled; }
	void setHasStroke(bool enabled) { m_hasStroke = enabled; }
	void lineWidth(double w);
	void masterAlpha(double a);

	void fillLinearGradient(double x1, double y1, double x2, double y2,
	                        MetalOSDColor c1, MetalOSDColor c2, double profile = 1.0);
	void lineLinearGradient(double x1, double y1, double x2, double y2,
	                        MetalOSDColor c1, MetalOSDColor c2, double profile = 1.0);

	// Fonts and text
	void setFont(const std::string &name);
	void renderText(double dstX, double dstY, const std::string &str);
	void renderTextDropshadowed(double dstX, double dstY, const std::string &str);

	// Primitives
	void line(double x1, double y1, double x2, double y2);
	void triangle(double x1, double y1, double x2, double y2, double x3, double y3);
	void rectangle(double x1, double y1, double x2, double y2);
	void roundedRect(double x1, double y1, double x2, double y2, double r);
	void roundedRect(double x1, double y1, double x2, double y2, double rx, double ry);
	void roundedRect(double x1, double y1, double x2, double y2,
	                 double rxBottom, double ryBottom, double rxTop, double ryTop);
	void ellipse(double cx, double cy, double rx, double ry);
	void arc(double cx, double cy, double rx, double ry, double start, double sweep);
	void star(double cx, double cy, double r1, double r2, double startAngle, int numRays);
	void curve(double x1, double y1, double x2, double y2, double x3, double y3);
	void polygon(double *xy, int numPoints);

private:
	uint32_t PackColor(MetalOSDColor c) const;
	void RecordShape(uint32_t type, double x0, double y0, double x1, double y1, double radius);
	void RecordPolygon(const MetalOSDPoint *points, int count);
	void RecordPolyline(const MetalOSDPoint *points, int count);
	void RecordText(double dstX, double dstY, const std::string &str);

	MetalOSDFrame m_frame;

	bool m_hasFill;
	bool m_hasStroke;
	bool m_fillGradient;
	bool m_lineGradient;
	MetalOSDColor m_fill0, m_fill1;
	MetalOSDColor m_line0, m_line1;
	double m_gradientX0, m_gradientY0, m_gradientX1, m_gradientY1;
	double m_lineGradientX0, m_lineGradientY0, m_lineGradientX1, m_lineGradientY1;
	double m_strokeWidth;
	double m_masterAlpha;
	const uint8_t *m_fontData;
};

// ---------------------------------------------------------------------------
// Frame exchange between the emulation thread (records) and the render thread
// (consumes).  Mirrors the double-buffered Lua overlay in MacLuaScriptConsole.mm.
// ---------------------------------------------------------------------------

MacMetalOSDDrawTarget *MacMetalOSD_BackTarget();   // emulation thread only
void MacMetalOSD_ClearBack();                     // call at the start of a frame
void MacMetalOSD_PresentBack();                   // call after all draws for the frame

// Cocoa replacement for gui.osdtext(): cocoa has no OSDCLASS, so this records
// one white drop-shadowed text run into the current frame.
void MacMetalOSD_AddFixedText(int x, int y, const char *text);

const MetalOSDFrame *MacMetalOSD_LockFront();     // render thread only
void MacMetalOSD_UnlockFront();
uint64_t MacMetalOSD_FrontGeneration();

#endif // _MAC_METAL_OSD_DRAW_H_
