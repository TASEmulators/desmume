--[[----------------------------------------------------------------------------
	agg.* smoke test - exercises every shape and text path of the OSD layer.

	Load it from the Lua Script dialog ("Run"), or with
	emu.openscript("osd-shapes.lua").  The upper screen shows a moving test
	pattern; on the macOS Metal OSD a second pattern is drawn on the touch
	screen as well (the AGG front ends clip everything below y = 191).

	What to look for:

	  * every shape in the "primitives" band is drawn, with antialiased edges,
	  * the star, the ellipse and the arc move smoothly and the frame counter
	    keeps counting (a frozen counter means the OSD is only presented once),
	  * the "paint state" widget must show a hollow rectangle followed by a
	    filled one - if the filled one is missing, noFill() was not undone by
	    agg.fillColor(),
	  * each font sample is legible and drop-shadowed.

	Drawing has to happen every frame: the engine clears the recorded frame at
	the start of each one, which is why the pattern is drawn from gui.register()
	rather than once from the script body.
------------------------------------------------------------------------------]]

local SHOW_TOUCH_HALF = true   -- Metal OSD only; harmless everywhere else
local FONT_SAMPLES = { "verdana18_bold", "gse8x16", "mcs5x10_mono", "gse5x7" }

local PI = math.pi

-- 0xRRGGBBAA, alpha in the low byte.  Plain arithmetic keeps this working on
-- both engines: a stock Lua 5.1 has no bit library until the emulator adds one,
-- and LuaJIT's is signed 32-bit.
local function rgb(r, g, b, a)
	return r * 0x1000000 + g * 0x10000 + b * 0x100 + (a or 255)
end

local COL_WHITE  = rgb(255, 255, 255)
local COL_DIM    = rgb(150, 150, 150)
local COL_YELLOW = rgb(255, 214, 64)
local COL_RED    = rgb(232, 64, 64)
local COL_GREEN  = rgb(64, 200, 120)
local COL_BLUE   = rgb(64, 120, 232)
local COL_ORANGE = rgb(255, 140, 40)
local COL_CYAN   = rgb(80, 220, 240)
local COL_MAGENTA = rgb(220, 80, 200)

-- A faint frame so the edges of the 256x192 area are visible.
local function drawBorder()
	agg.noFill()
	agg.lineColor(COL_DIM)
	agg.lineWidth(1)
	agg.rectangle(0, 0, 255, 191)
end

local function drawHeader(frame)
	agg.noFill()
	agg.lineColor(COL_WHITE)
	agg.lineWidth(1)
	agg.rectangle(2, 2, 253, 40)

	agg.lineColor(COL_WHITE)
	agg.setFont("verdana18_bold")
	agg.text(8, 20, "DeSmuME OSD test")

	agg.setFont("gse5x7")
	agg.lineColor(COL_YELLOW)
	agg.text(8, 32, "agg.* shapes + text")

	agg.lineColor(COL_CYAN)
	agg.text(196, 32, string.format("f%d", frame))
end

-- Static geometry, so a missing primitive is obvious at a glance.
local function drawPrimitives()
	agg.setFont("gse5x7")

	-- filled rectangle with a 2px white stroke
	agg.fillColor(COL_BLUE)
	agg.lineColor(COL_WHITE)
	agg.lineWidth(2)
	agg.rectangle(4, 46, 40, 84)

	-- rounded rectangle, fill only
	agg.fillColor(COL_ORANGE)
	agg.noLine()
	agg.roundedRect(46, 46, 82, 84, 9)

	-- ellipse, stroke only
	agg.noFill()
	agg.lineColor(COL_GREEN)
	agg.lineWidth(2)
	agg.ellipse(96, 65, 12, 19)

	-- triangle
	agg.fillColor(COL_RED)
	agg.lineColor(COL_WHITE)
	agg.lineWidth(1)
	agg.triangle(110, 84, 128, 46, 146, 84)

	-- star: outer radius 18, inner 8, five points
	agg.fillColor(COL_YELLOW)
	agg.lineColor(rgb(140, 90, 0))
	agg.lineWidth(1)
	agg.star(168, 65, 8, 18, 0, 5)

	-- half circle drawn as an arc
	agg.noFill()
	agg.lineColor(COL_MAGENTA)
	agg.lineWidth(2)
	agg.arc(218, 65, 20, 16, 0, PI)

	-- captions
	agg.lineColor(COL_DIM)
	agg.text(10, 88, "rect")
	agg.text(50, 88, "rrect")
	agg.text(89, 88, "ell")
	agg.text(112, 88, "tri")
	agg.text(161, 88, "star")
	agg.text(206, 88, "arc")
end

-- Shapes whose parameters change every frame.
local function drawAnimation(t)
	-- The bezier's control point orbits, which makes the curve breathe.
	local cx = 128 + math.cos(t) * 40
	local cy = 116 + math.sin(t * 0.7) * 14
	agg.lineColor(COL_CYAN)
	agg.lineWidth(2)
	agg.noFill()
	agg.curve(6, 118, cx, cy, 124, 112)

	-- Stroke width ladder, plus a moving segment.
	agg.lineColor(COL_WHITE)
	for i = 1, 4 do
		agg.lineWidth(i)
		agg.line(132, 104 + i * 4, 176, 104 + i * 4)
	end

	-- A filled bar that slides back and forth.
	local slide = 128 + math.sin(t) * 60
	agg.fillColor(COL_GREEN)
	agg.noLine()
	agg.rectangle(slide - 8, 130, slide + 8, 138)

	-- Pulses between outline-only and filled by re-setting the paint state.
	agg.fillColor(rgb(255, 255, 255, 40 + 60 * (1 + math.sin(t * 1.5)) / 2))
	agg.lineColor(COL_ORANGE)
	agg.lineWidth(1)
	agg.roundedRect(190, 100, 246, 136, 6 + 4 * (1 + math.sin(t * 2)) / 2)
end

-- Regression guard for the paint-state machine: a hollow rectangle followed by
-- a filled one.  If the filled one disappears, fillColor() stopped re-enabling
-- the fill after noFill().
local function drawPaintStateWidget()
	agg.setFont("gse5x7")
	agg.lineColor(COL_DIM)
	agg.text(8, 148, "paint state")

	agg.noFill()
	agg.lineColor(COL_WHITE)
	agg.lineWidth(1)
	agg.rectangle(8, 152, 34, 176)

	agg.fillColor(COL_BLUE)   -- must re-enable the fill
	agg.noLine()
	agg.rectangle(40, 152, 66, 176)

	agg.noLine()
	agg.lineColor(COL_CYAN)   -- must re-enable the stroke
	agg.lineWidth(2)
	agg.line(70, 176, 96, 152)

	agg.fillColor(COL_ORANGE)
	agg.noLine()
	agg.rectangle(100, 152, 126, 176)
end

-- One font per 90 frames, so all of them get looked at.
local function drawFontSample(frame)
	local font = FONT_SAMPLES[(math.floor(frame / 90) % #FONT_SAMPLES) + 1]
	agg.setFont(font)
	agg.lineColor(COL_WHITE)
	agg.text(150, 176, "Lua 0123")

	agg.setFont("gse5x7")
	agg.lineColor(COL_DIM)
	agg.text(150, 152, font)
end

-- The lower half of the OSD frame; only visible on the Metal backend, where
-- the touch screen gets its own half of the 256x384 HUD texture.
local function drawTouchHalf(t)
	agg.setFont("verdana16_bold")
	agg.lineColor(COL_WHITE)
	agg.text(10, 216, "touch screen")
	agg.setFont("gse5x7")
	agg.lineColor(COL_DIM)
	agg.text(10, 230, "only drawn on the Metal OSD")

	agg.fillColor(COL_CYAN)
	agg.noLine()
	agg.ellipse(60, 300, 40, 34)

	agg.noFill()
	agg.lineColor(COL_YELLOW)
	agg.lineWidth(3)
	agg.arc(60, 300, 46, 40, 0, PI * 1.5)

	agg.fillColor(COL_MAGENTA)
	agg.lineColor(COL_WHITE)
	agg.lineWidth(1)
	agg.star(170, 300, 12, 34, t, 7)

	-- A row of coloured squares: the alpha ramp checks premultiplied blending.
	for i = 0, 7 do
		agg.fillColor(rgb(255, 255, 255, 32 + i * 32))
		agg.noLine()
		agg.rectangle(150 + i * 12, 350, 160 + i * 12, 364)
	end

	agg.fillColor(COL_WHITE)
end

local function hud()
	local frame = emu.framecount()
	local t = frame * 0.03

	drawBorder()
	drawHeader(frame)
	drawPrimitives()
	drawAnimation(t)
	drawPaintStateWidget()
	drawFontSample(frame)

	if SHOW_TOUCH_HALF then
		drawTouchHalf(t)
	end

	agg.setFont("gse5x7")
	agg.lineColor(COL_WHITE)
	agg.noFill()
	agg.lineWidth(1)
end

gui.register(hud)

while true do
	emu.frameadvance()
end
