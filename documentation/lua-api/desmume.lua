---@meta

--[[----------------------------------------------------------------------------
	Type definitions for the DeSmuME Lua scripting API (Lua Language Server).

	This file is not loaded by the emulator and is not part of the build.  It
	only tells the editor what the Lua state created in
	desmume/src/lua-engine.cpp actually contains, so that a script gets
	completion, parameter names and types, and errors for functions that do not
	exist or that are called with the wrong argument types.

	The names below are one-to-one with the luaL_Reg tables at the end of
	lua-engine.cpp.  Run documentation/lua-api/check_api.py after changing them.
------------------------------------------------------------------------------]]

-- ===========================================================================
-- Shared types
-- ===========================================================================

--- A colour.  Accepted everywhere a colour parameter appears:
--- * a number in `0xRRGGBBAA` form (alpha in the **low** byte, `0xFFFFFFFF` is opaque white),
--- * a name: `"white"`, `"black"`, `"clear"`, `"gray"`/`"grey"`, `"red"`, `"orange"`,
---   `"yellow"`, `"chartreuse"`, `"green"`, `"teal"`, `"cyan"`, `"blue"`, `"purple"`, `"magenta"`,
--- * a string in `"#RRGGBB"` / `"#RRGGBBAA"` form,
--- * a table `{r=0, g=0, b=0, a=255}` (keys `r`/`g`/`b`/`a` or `1`/`2`/`3`/`4`, each 0-255),
--- * a function returning any of the above.
---
--- The alpha of the result is additionally scaled by `gui.opacity()` /
--- `gui.transparency()` for every `gui.*` drawing function; `agg.*` colours are
--- used verbatim.
---@alias DsColor integer|string|DsColorTable|fun():integer

---@class DsColorTable
---@field r? integer red, 0-255
---@field g? integer green, 0-255
---@field b? integer blue, 0-255
---@field a? integer alpha, 0-255
---@field [1]? integer red, 0-255
---@field [2]? integer green, 0-255
---@field [3]? integer blue, 0-255
---@field [4]? integer alpha, 0-255

--- The buttons of one NDS pad.  `true` = held, `false` = released.
--- `"debug"` is not a real button; it exists so that `joypad.read()` can report
--- the front end's debug key, and is listed here for symmetry with the C table.
---@class DsButtonTable
---@field A? boolean
---@field B? boolean
---@field X? boolean
---@field Y? boolean
---@field L? boolean
---@field R? boolean
---@field start? boolean
---@field select? boolean
---@field up? boolean
---@field down? boolean
---@field left? boolean
---@field right? boolean
---@field lid? boolean
---@field debug? boolean

--- Touch screen state.
---@class DsTouchState
---@field x? integer x in screen pixels, 0-255
---@field y? integer y in screen pixels, 0-191
---@field touch? boolean whether the screen is being pressed

--- A savestate handle.  `savestate.create()` without an argument returns an
--- anonymous in-memory state (userdata); with a number it returns that number,
--- which `savestate.save()`/`savestate.load()` also accept.
---@alias DsSaveState integer|userdata

--- A GD-format image string, as produced by `gui.gdscreenshot()`.
---@alias DsGdImage string

-- ===========================================================================
-- agg — vector drawing on the OSD layer
-- ===========================================================================

--- Vector drawing above the emulated screens.
---
--- Coordinates are the pixels of one emulated screen: origin at the top left,
--- x 0-255, y 0-191.  macOS renders this through Metal, Windows and Linux
--- through the bundled AGG rasteriser; the API is the same on all three.
---
--- The paint state (`fillColor`, `lineColor`, `noFill`, `noLine`, `lineWidth`,
--- `setFont`) persists across calls and across frames.  Text is drawn in
--- `lineColor`, and setting a colour re-enables the matching paint after
--- `noFill()`/`noLine()`.
---
--- Calls are recorded into the frame that is currently being emulated and are
--- discarded at the start of the next one, so a script has to keep drawing
--- every frame - from a `gui.register` callback, or right before
--- `emu.frameadvance()` - for the picture to stay on screen.
agg = {}

--- Draws a line.
---@param x1 number
---@param y1 number
---@param x2 number
---@param y2 number
function agg.line(x1, y1, x2, y2) end

--- Draws a triangle.
---@param x1 number
---@param y1 number
---@param x2 number
---@param y2 number
---@param x3 number
---@param y3 number
function agg.triangle(x1, y1, x2, y2, x3, y3) end

--- Draws an axis-aligned rectangle from two opposite corners.
---@param x1 number
---@param y1 number
---@param x2 number
---@param y2 number
function agg.rectangle(x1, y1, x2, y2) end

--- Draws a rectangle with rounded corners.
---@param x1 number
---@param y1 number
---@param x2 number
---@param y2 number
---@param r number corner radius in pixels
function agg.roundedRect(x1, y1, x2, y2, r) end

--- Draws an ellipse.  A circle is `agg.ellipse(cx, cy, r, r)`.
---@param cx number centre x
---@param cy number centre y
---@param rx number horizontal radius
---@param ry number vertical radius
function agg.ellipse(cx, cy, rx, ry) end

--- Draws an elliptical arc.  An arc is an open path, so it only ever appears as
--- a stroke and is skipped after `agg.noLine()`.
---@param cx number centre x
---@param cy number centre y
---@param rx number horizontal radius
---@param ry number vertical radius
---@param start number start angle in radians
---@param sweep number sweep angle in radians, may be negative
function agg.arc(cx, cy, rx, ry, start, sweep) end

--- Draws a star.  `r2` is the outer radius, `r1` the inner radius.
---@param cx number centre x
---@param cy number centre y
---@param r1 number inner radius
---@param r2 number outer radius
---@param startAngle number start angle in radians
---@param numRays integer number of points, at most 32
function agg.star(cx, cy, r1, r2, startAngle, numRays) end

--- Draws a quadratic Bezier curve.  Open path: stroke only.
---@param x1 number start x
---@param y1 number start y
---@param x2 number control point x
---@param y2 number control point y
---@param x3 number end x
---@param y3 number end y
function agg.curve(x1, y1, x2, y2, x3, y3) end

--- Sets the fill colour, and re-enables filling after `agg.noFill()`.
---@param color DsColor
function agg.fillColor(color) end

--- Disables filling until the next `agg.fillColor()`.
function agg.noFill() end

--- Sets the stroke and text colour, and re-enables stroking after `agg.noLine()`.
---@param color DsColor
function agg.lineColor(color) end

--- Disables stroking until the next `agg.lineColor()`.
function agg.noLine() end

--- Sets the stroke width in pixels.  The stroke is centred on the path.
---@param width number
function agg.lineWidth(width) end

--- Selects the bitmap font used by `agg.text()`; an unknown name keeps the
--- current font.  Available names:
--- `gse4x6`, `gse4x8`, `gse5x7`, `gse5x9`, `gse6x9`, `gse6x12`, `gse7x11`,
--- `gse7x11_bold`, `gse7x15`, `gse7x15_bold`, `gse8x16`, `gse8x16_bold`,
--- `mcs11_prop`, `mcs11_prop_condensed`, `mcs12_prop`, `mcs13_prop`,
--- `mcs5x10_mono`, `mcs5x11_mono`, `mcs6x10_mono`, `mcs6x11_mono`,
--- `mcs7x12_mono_high`, `mcs7x12_mono_low`, `verdana12`, `verdana12_bold`,
--- `verdana13`, `verdana13_bold`, `verdana14`, `verdana14_bold`, `verdana16`,
--- `verdana16_bold`, `verdana17`, `verdana17_bold`, `verdana18`, `verdana18_bold`.
---
--- The default is `verdana18_bold`.
---@param name string
function agg.setFont(name) end

--- Draws text with a one-pixel drop shadow, at `(x, y)` minus the font's base
--- line.  The colour is the current `agg.lineColor()`; the shadow is black for a
--- light colour and the inverse colour for a dark one.
---@param x integer
---@param y integer
---@param str string
function agg.text(x, y, str) end

-- ===========================================================================
-- gui — the pixel canvas
-- ===========================================================================

--- Drawing on a 256x384 canvas composited over both NDS screens: y 0-191 is the
--- upper screen, y 192-383 the touch screen.
---
--- `gui.text()` uses the emulator's built-in bitmap font and is ASCII only: a
--- line that contains a non-ASCII byte is dropped as a whole.
---
--- Drawing calls made when the emulator is not at a frame boundary are deferred
--- to the end of the frame instead of being applied immediately, so they work
--- from the body of a script as well as from a `gui.register` callback.
gui = {}

--- Registers a function that is called after every emulated frame, before the
--- canvas is composited.  This is where drawing calls belong.
---@param func fun():nil
function gui.register(func) end

--- Draws a string.
---@param x integer
---@param y integer
---@param str string ASCII only
---@param color? DsColor defaults to white
---@param outline? DsColor defaults to black
function gui.text(x, y, str, color, outline) end

--- Draws a filled and outlined rectangle.
---@param x1 integer
---@param y1 integer
---@param x2 integer
---@param y2 integer
---@param fill? DsColor defaults to 0xFFFFFF3F
---@param outline? DsColor defaults to the fill colour with alpha forced to 255
function gui.box(x1, y1, x2, y2, fill, outline) end

--- Draws a line.
---@param x1 integer
---@param y1 integer
---@param x2 integer
---@param y2 integer
---@param color? DsColor defaults to white
---@param skipfirst? boolean skip the first pixel, defaults to false
function gui.line(x1, y1, x2, y2, color, skipfirst) end

--- Draws a single pixel.  A fully transparent colour draws nothing.
---@param x integer
---@param y integer
---@param color? DsColor defaults to white
function gui.pixel(x, y, color) end

--- Reads a pixel back from the canvas.
---@param x integer
---@param y integer
---@return integer r 0-255
---@return integer g 0-255
---@return integer b 0-255
function gui.getpixel(x, y) end

--- Sets the alpha multiplier (0.0-1.0) applied to every colour passed to `gui.*`.
---@param alpha number 0.0 transparent, 1.0 opaque; values above 1.0 are allowed
function gui.opacity(alpha) end

--- Sets the alpha multiplier on the 4-0 scale of the TASVideos VBA docs:
--- 4 is opaque, 0 is invisible.
---@param transparency integer
function gui.transparency(transparency) end

--- Shows a modal message box.
---@param message string
---@param type? string "ok", "yesno", "yesnocancel", "okcancel" or "abortretryignore"; defaults to "ok"
---@param icon? string "message", "question", "warning" or "error"; defaults to "message"
---@return string answer "ok", "cancel", "abort", "retry", "ignore", "yes" or "no"
function gui.popup(message, type, icon) end

--- Splits a colour into channels, without applying the opacity multiplier.
---@param color DsColor
---@return integer r 0-255
---@return integer g 0-255
---@return integer b 0-255
---@return integer a 0-255
function gui.parsecolor(color) end

--- Chooses which screens the canvas is drawn on.
---@param main boolean draw on the upper screen
---@param sub boolean draw on the touch screen
function gui.setlayermask(main, sub) end

--- Captures the canvas as a GD-format image string.
---@param whichScreen? string|integer|boolean "both" (default), "top", "bottom"
---@return DsGdImage
function gui.gdscreenshot(whichScreen) end

--- Blits a GD-format image onto the canvas.
---@param dx? integer destination x, defaults to 0
---@param dy? integer destination y, defaults to 0
---@param gdimage DsGdImage
---@param sx? integer source x
---@param sy? integer source y
---@param width? integer source width
---@param height? integer source height
---@param alphamul? integer extra alpha multiplier for the source
function gui.gdoverlay(dx, dy, gdimage, sx, sy, width, height, alphamul) end

--- Redraws the emulator window immediately; same as `emu.redraw()`.
function gui.redraw() end

--- Draws text with the emulator's own HUD font and shadow, outside the Lua
--- canvas.  On the macOS front end this goes through the Metal OSD, on the AGG
--- front ends through the OSD object.
---@param x integer
---@param y integer
---@param msg string
function gui.osdtext(x, y, msg) end

--- Alias of `gui.text()`.
---@param x integer
---@param y integer
---@param str string
---@param color? DsColor
---@param outline? DsColor
function gui.drawtext(x, y, str, color, outline) end

--- Alias of `gui.box()`.
---@param x1 integer
---@param y1 integer
---@param x2 integer
---@param y2 integer
---@param fill? DsColor
---@param outline? DsColor
function gui.drawbox(x1, y1, x2, y2, fill, outline) end

--- Alias of `gui.line()`.
---@param x1 integer
---@param y1 integer
---@param x2 integer
---@param y2 integer
---@param color? DsColor
---@param skipfirst? boolean
function gui.drawline(x1, y1, x2, y2, color, skipfirst) end

--- Alias of `gui.pixel()`.
---@param x integer
---@param y integer
---@param color? DsColor
function gui.drawpixel(x, y, color) end

--- Alias of `gui.pixel()`.
---@param x integer
---@param y integer
---@param color? DsColor
function gui.setpixel(x, y, color) end

--- Alias of `gui.pixel()`.
---@param x integer
---@param y integer
---@param color? DsColor
function gui.writepixel(x, y, color) end

--- Alias of `gui.getpixel()`.
---@param x integer
---@param y integer
---@return integer r
---@return integer g
---@return integer b
function gui.readpixel(x, y) end

--- Alias of `gui.box()`.
---@param x1 integer
---@param y1 integer
---@param x2 integer
---@param y2 integer
---@param fill? DsColor
---@param outline? DsColor
function gui.rect(x1, y1, x2, y2, fill, outline) end

--- Alias of `gui.box()`.
---@param x1 integer
---@param y1 integer
---@param x2 integer
---@param y2 integer
---@param fill? DsColor
---@param outline? DsColor
function gui.drawrect(x1, y1, x2, y2, fill, outline) end

--- Alias of `gui.gdoverlay()`.
---@param dx? integer
---@param dy? integer
---@param gdimage DsGdImage
---@param sx? integer
---@param sy? integer
---@param width? integer
---@param height? integer
---@param alphamul? integer
function gui.drawimage(dx, dy, gdimage, sx, sy, width, height, alphamul) end

--- Alias of `gui.gdoverlay()`.
---@param dx? integer
---@param dy? integer
---@param gdimage DsGdImage
---@param sx? integer
---@param sy? integer
---@param width? integer
---@param height? integer
---@param alphamul? integer
function gui.image(dx, dy, gdimage, sx, sy, width, height, alphamul) end

-- ===========================================================================
-- emu — emulation control
-- ===========================================================================

--- Emulation control and lifecycle callbacks.
---
--- A script runs on the emulation thread.  `emu.frameadvance()` hands control
--- back to the emulator for one frame and returns at the next frame boundary;
--- everything else is a synchronous query or a registration.
emu = {}

--- Advances one frame and waits until it has been drawn.
function emu.frameadvance() end

--- Advances one frame at normal speed.
function emu.emulateframe() end

--- Advances one frame as fast as possible, skipping frames if configured.
function emu.emulateframefast() end

--- Advances one frame as fast as possible but still renders every frame.
function emu.emulateframefastnoskipping() end

--- Advances one frame without rendering graphics or generating sound.
function emu.emulateframeinvisible() end

--- Sets the speed used by `emu.frameadvance()`.
---@param mode string|integer "normal", "nothrottle", "turbo" or "maximum"
function emu.speedmode(mode) end

--- Waits until the emulator can accept input again, without advancing a frame.
function emu.wait() end

--- Pauses the emulator.
function emu.pause() end

--- Resumes the emulator.
function emu.unpause() end

--- Requests an immediate redraw of the emulator window.
function emu.redraw() end

--- Resets the emulated machine.
function emu.reset() end

--- Shows a message in the front end's status area.
---@param str string
function emu.message(str) end

--- Prints its arguments to the script console.  The engine replaces the global
--- `print` with this function.
---@param ... any
function emu.print(...) end

--- Returns the 4-character game code of the loaded ROM.
---@return string
function emu.gamecode() end

--- Returns the first 3 characters of the game code.
---@return string
function emu.smallgamecode() end

--- Returns the number of frames emulated since the ROM was loaded.
---@return integer
function emu.framecount() end

--- Returns the number of lag frames counted so far.
---@return integer
function emu.lagcount() end

--- Returns whether the frame currently being emulated is a lag frame.
---@return boolean
function emu.lagged() end

--- Returns whether emulation has started (a ROM is loaded and running).
---@return boolean
function emu.emulating() end

--- Returns whether the emulator is at a frame boundary, that is, whether
--- drawing and input calls take effect immediately.
---@return boolean
function emu.atframeboundary() end

--- Registers a function that runs before each frame is emulated.
---@param func fun():nil
function emu.registerbefore(func) end

--- Registers a function that runs after each frame is emulated.
---@param func fun():nil
function emu.registerafter(func) end

--- Registers a function that runs when emulation starts.
---@param func fun():nil
function emu.registerstart(func) end

--- Registers a function that runs when the script is stopped or the emulator
--- exits.
---@param func fun():nil
function emu.registerexit(func) end

--- Keeps the named globals across a `savestate.save()`/`savestate.load()` round
--- trip.  Only numbers, strings, booleans and tables of those are stored.
---@param variabletable table<string, any>
function emu.persistglobalvariables(variabletable) end

--- Runs another script file in place of the current one.
---@param filename string
function emu.openscript(filename) end

--- Appends a menu to the front end's menu bar.  Windows only; a no-op on macOS
--- and Linux.
---@param menuName string
---@param menuEntries table<string, fun():nil>|table[] map of label to callback, or an array of nested tables
function emu.addmenu(menuName, menuEntries) end

--- Changes the properties of a menu item created by `emu.addmenu()`.  Windows
--- only; a no-op on macOS and Linux.
---@param menuItem fun():nil the callback that identifies the item
---@param infoTable table<string, any> for example `{ checked = true, enabled = false }`
function emu.setmenuiteminfo(menuItem, infoTable) end

--- Registers a function called once the menu has been built.  Windows only.
---@param func? fun():nil
function emu.registermenustart(func) end

--- Sets the freelook transformation applied to the 3D layers.
---@param mode integer 0 = off, 2 or 3 = use `matrix`
---@param matrix? number[] 16 consecutive numbers, read when `mode` is 2 or 3
function emu.set3dtransform(mode, matrix) end

--- Registers a function called every time the 3D layers are rendered.
---@param func? fun():nil
function emu.register3devent(func) end

-- ===========================================================================
-- memory — the emulated address space and CPU registers
-- ===========================================================================

--- Byte/word/dword access to the ARM9 address space, plus CPU registers.
---
--- Every access goes through the emulated MMU, so it behaves exactly like an
--- access from the emulated CPU.  The signed variants sign-extend their result.
memory = {}

--- Reads one byte.
---@param address integer
---@return integer 0-255
function memory.readbyte(address) end

--- Reads one byte, sign-extended.
---@param address integer
---@return integer -128 to 127
function memory.readbytesigned(address) end

--- Reads one halfword (16 bit).
---@param address integer
---@return integer 0-65535
function memory.readword(address) end

--- Reads one halfword, sign-extended.
---@param address integer
---@return integer -32768 to 32767
function memory.readwordsigned(address) end

--- Reads one word (32 bit).
---@param address integer
---@return integer 0-4294967295
function memory.readdword(address) end

--- Reads one word, sign-extended.
---@param address integer
---@return integer
function memory.readdwordsigned(address) end

--- Reads a range of bytes.
---@param address integer first address
---@param length integer byte count; a negative value starts at `address + length` and reads forward
---@return integer[] bytes, `t[1]` is the byte at `address`
function memory.readbyterange(address, length) end

--- Writes one byte.
---@param address integer
---@param value integer only the low 8 bits are used
function memory.writebyte(address, value) end

--- Writes one halfword.
---@param address integer
---@param value integer only the low 16 bits are used
function memory.writeword(address, value) end

--- Writes one word.
---@param address integer
---@param value integer only the low 32 bits are used
function memory.writedword(address, value) end

--- Returns whether an address is mapped in the current memory map.
---@param address integer
---@return boolean
function memory.isvalid(address) end

--- Reads a register by name, for example `"arm9.r0"`, `"arm7.pc"`,
--- `"main.cpsr"` or `"sub.sp"`.  The CPU prefix may be omitted, in which case
--- ARM9 is used.
---@param name string
---@return integer|nil value, or nil for an unknown register name
function memory.getregister(name) end

--- Writes a register by name.
---@param name string see `memory.getregister()`
---@param value integer
function memory.setregister(name, value) end

--- Reads a halfword directly out of VRAM.
---@param address integer
---@return integer 0-65535
function memory.vram_readword(address) end

--- Writes a halfword directly into VRAM.
---@param address integer
---@param value integer only the low 16 bits are used
function memory.vram_writeword(address, value) end

--- Calls `func` whenever the emulated CPU writes into the watched range.
---@param address integer first address of the range
---@param size? integer bytes to watch (1, 2 or 4), defaults to 1
---@param cpuname? string "main" (default) or "sub"
---@param func fun(address:integer, size:integer, value:integer, cpuname:string)
function memory.registerwrite(address, size, cpuname, func) end

--- Calls `func` whenever the emulated CPU reads from the watched range.
---@param address integer first address of the range
---@param size? integer bytes to watch, defaults to 1
---@param cpuname? string "main" (default) or "sub"
---@param func fun(address:integer, size:integer, cpuname:string)
function memory.registerread(address, size, cpuname, func) end

--- Calls `func` whenever the emulated CPU executes inside the watched range.
---@param address integer first address of the range
---@param size? integer bytes to watch, defaults to 2
---@param cpuname? string "main" (default) or "sub"
---@param func fun(address:integer, size:integer, cpuname:string)
function memory.registerexec(address, size, cpuname, func) end

--- Alias of `memory.registerwrite()`.
---@param address integer
---@param size? integer
---@param cpuname? string
---@param func fun(address:integer, size:integer, value:integer, cpuname:string)
function memory.register(address, size, cpuname, func) end

--- Alias of `memory.registerexec()`.
---@param address integer
---@param size? integer
---@param cpuname? string
---@param func fun(address:integer, size:integer, cpuname:string)
function memory.registerrun(address, size, cpuname, func) end

--- Alias of `memory.registerexec()`.
---@param address integer
---@param size? integer
---@param cpuname? string
---@param func fun(address:integer, size:integer, cpuname:string)
function memory.registerexecute(address, size, cpuname, func) end

--- Alias of `memory.readbyte()`.
---@param address integer
---@return integer
function memory.readbyteunsigned(address) end

--- Alias of `memory.readword()`.
---@param address integer
---@return integer
function memory.readwordunsigned(address) end

--- Alias of `memory.readdword()`.
---@param address integer
---@return integer
function memory.readdwordunsigned(address) end

--- Alias of `memory.readword()`.
---@param address integer
---@return integer
function memory.readshort(address) end

--- Alias of `memory.readword()`.
---@param address integer
---@return integer
function memory.readshortunsigned(address) end

--- Alias of `memory.readwordsigned()`.
---@param address integer
---@return integer
function memory.readshortsigned(address) end

--- Alias of `memory.readdword()`.
---@param address integer
---@return integer
function memory.readlong(address) end

--- Alias of `memory.readdword()`.
---@param address integer
---@return integer
function memory.readlongunsigned(address) end

--- Alias of `memory.readdwordsigned()`.
---@param address integer
---@return integer
function memory.readlongsigned(address) end

--- Alias of `memory.writeword()`.
---@param address integer
---@param value integer
function memory.writeshort(address, value) end

--- Alias of `memory.writedword()`.
---@param address integer
---@param value integer
function memory.writelong(address, value) end

-- ===========================================================================
-- joypad — the emulated pad
-- ===========================================================================

--- Reads and overrides the NDS buttons.
---
--- `joypad.get()`/`joypad.read()` report what the game sees (including a
--- playing movie); `joypad.peek()` reports what the user is physically pressing.
joypad = {}

--- Returns the state of every button.
---@param controller? integer controller index, defaults to 0
---@return DsButtonTable
function joypad.get(controller) end

--- Returns only the buttons that are currently held.
---@param controller? integer
---@return DsButtonTable
function joypad.getdown(controller) end

--- Returns only the buttons that are currently released.
---@param controller? integer
---@return DsButtonTable
function joypad.getup(controller) end

--- Returns the physically held buttons, ignoring movies and input filters.
---@param controller? integer
---@return DsButtonTable
function joypad.peek(controller) end

--- Returns the physically held buttons only.
---@param controller? integer
---@return DsButtonTable
function joypad.peekdown(controller) end

--- Returns the physically released buttons only.
---@param controller? integer
---@return DsButtonTable
function joypad.peekup(controller) end

--- Sets the pad state for the next frame.  `true` presses, `false` releases and
--- an absent field is left unchanged.  Ignored while a movie is playing.
---@param controller? integer controller index, defaults to 0
---@param buttons DsButtonTable
function joypad.set(controller, buttons) end

--- Alias of `joypad.get()`.
---@param controller? integer
---@return DsButtonTable
function joypad.read(controller) end

--- Alias of `joypad.set()`.
---@param controller? integer
---@param buttons DsButtonTable
function joypad.write(controller, buttons) end

--- Alias of `joypad.getdown()`.
---@param controller? integer
---@return DsButtonTable
function joypad.readdown(controller) end

--- Alias of `joypad.getup()`.
---@param controller? integer
---@return DsButtonTable
function joypad.readup(controller) end

-- ===========================================================================
-- stylus — the touch screen
-- ===========================================================================

--- Reads and overrides the touch screen.  Coordinates are pixels of the touch
--- screen, 0-255 by 0-191.
stylus = {}

--- Returns the touch state the game saw on the last frame.
---@return DsTouchState
function stylus.get() end

--- Returns the touch state the user is requesting right now.
---@return DsTouchState
function stylus.peek() end

--- Sets the touch state for the next frame, for example
--- `stylus.set{x=128, y=96, touch=true}`.  Ignored while a movie is playing.
---@param state DsTouchState
function stylus.set(state) end

--- Alias of `stylus.get()`.
---@return DsTouchState
function stylus.read() end

--- Alias of `stylus.set()`.
---@param state DsTouchState
function stylus.write(state) end

-- ===========================================================================
-- savestate
-- ===========================================================================

--- Saves and loads emulator states, either into numbered slots or into
--- anonymous in-memory buffers.
savestate = {}

--- Creates a savestate handle.  With a number that number is returned as-is and
--- selects a numbered slot; without one, an anonymous in-memory state is
--- allocated.
---@param location? integer
---@return DsSaveState
function savestate.create(location) end

--- Saves the current state.
---@param location DsSaveState numbered slot or handle from `savestate.create()`
---@param option? string "quiet" or "scriptdataonly"
function savestate.save(location, option) end

--- Loads a state.
---@param location DsSaveState
---@param option? string "quiet" or "scriptdataonly"
function savestate.load(location, option) end

--- Compares the current state with the given one and raises an error when they
--- differ.  Only registered in non-release builds, which includes the macOS
--- debug builds.
---@param location DsSaveState
---@param option? string
function savestate.verify(location, option) end

-- ===========================================================================
-- movie
-- ===========================================================================

--- Movie (TAS input log) control.
movie = {}

--- Returns whether a movie is loaded at all.
---@return boolean
function movie.active() end

--- Returns whether a movie is being recorded.
---@return boolean
function movie.recording() end

--- Returns whether a movie is being played back.
---@return boolean
function movie.playing() end

--- Returns the mode: "playback", "record", "inactive" or "finished".
---@return string
function movie.mode() end

--- Returns the number of frames in the loaded movie.
---@return integer
function movie.length() end

--- Returns the file name of the loaded movie, or "" when there is none.
---@return string
function movie.name() end

--- Returns the re-record count of the current movie.
---@return integer
function movie.rerecordcount() end

--- Sets the re-record count.
---@param count integer
function movie.setrerecordcount(count) end

--- Enables or disables re-record counting, or queries it.
---@param enabled? boolean
---@return boolean|nil current value when called without arguments
function movie.rerecordcounting(enabled) end

--- Returns whether the loaded movie is read-only.
---@return boolean
function movie.readonly() end

--- Makes the current movie read-only, or writable again.
---@param readonly boolean
function movie.setreadonly(readonly) end

--- Returns the current frame count; same as `emu.framecount()`.
---@return integer
function movie.framecount() end

--- Starts playing a movie.  Without an argument the front end asks for a file.
---@param filename? string
function movie.play(filename) end

--- Restarts the loaded movie from the beginning.
function movie.replay() end

--- Stops recording or playback.
function movie.stop() end

--- Alias of `movie.play()`.
---@param filename? string
function movie.open(filename) end

--- Alias of `movie.stop()`.
function movie.close() end

--- Alias of `movie.name()`.
---@return string
function movie.getname() end

--- Alias of `movie.play()`.
---@param filename? string
function movie.playback(filename) end

--- Alias of `movie.readonly()`.
---@return boolean
function movie.getreadonly() end

-- ===========================================================================
-- input — keyboard and mouse
-- ===========================================================================

--- Physical keyboard and mouse input as seen by the front end.
input = {}

--- Returns the pressed keys as a table of `keyName = true`.
---
--- Only the Windows front end fills this in; on macOS and Linux the table is
--- empty.
---@return table<string, boolean>
function input.get() end

--- Registers a callback for a hotkey.
---@param keynum integer front end specific key code
---@param func fun():nil
function input.registerhotkey(keynum, func) end

--- Shows a question box and returns the answer.
---@param message string
---@param type? string defaults to "yesno"
---@param icon? string defaults to "question"
---@return string answer "yes", "no", "ok", "cancel", "abort", "retry" or "ignore"
function input.popup(message, type, icon) end

--- Alias of `input.get()`.
---@return table<string, boolean>
function input.read() end

-- ===========================================================================
-- controller — raw joystick input
-- ===========================================================================

--- Raw controller state from the host, independent of the emulated pad.
---
--- Only the Windows front end reads a real joystick; elsewhere
--- `controller.get()` returns an empty table.
controller = {}

--- Returns the axis and button state of a joystick.
---@param joystickID? integer 0-15, defaults to 0 (the preferred device)
---@param returnDiagonals? boolean include diagonal D-pad states, defaults to false
---@return table<string, number|boolean> for example `h`, `v`, `x`, `y`, `z`, `r`, `u`, `pov`, `button1`
function controller.get(joystickID, returnDiagonals) end

--- Alias of `controller.get()`.
---@param joystickID? integer
---@param returnDiagonals? boolean
---@return table<string, number|boolean>
function controller.read(joystickID, returnDiagonals) end

-- ===========================================================================
-- sound
-- ===========================================================================

--- Audio control.
sound = {}

--- Empties the audio output buffer.
function sound.clear() end

-- ===========================================================================
-- Global functions
-- ===========================================================================

-- `print` and `tostring` are replaced by the engine's own implementations (they
-- write to the script console and never fail on tables with cycles), but their
-- signatures are the standard ones, so they are not redeclared here.

--- Bitwise AND of all arguments, in 32-bit unsigned arithmetic.
---@param x1 integer
---@param ... integer
---@return integer
function AND(x1, ...) end

--- Bitwise OR of all arguments.
---@param x1 integer
---@param ... integer
---@return integer
function OR(x1, ...) end

--- Bitwise XOR of all arguments.
---@param x1 integer
---@param ... integer
---@return integer
function XOR(x1, ...) end

--- Shifts `num` by `shift` bits: positive shifts left, negative shifts right.
---@param num integer
---@param shift integer
---@return integer
function SHIFT(num, shift) end

--- Returns `2^whichbit`, that is, a mask with only that bit set.
---@param whichbit integer 0-31
---@return integer
function BIT(whichbit) end

--- Returns the address of a table or a function as a number.  The value is only
--- unique while the object is alive.
---@param table_or_function table|function
---@return integer
function addressof(table_or_function) end

--- Deep-copies a table (numbers, strings, booleans and nested tables).
---@param origtable table
---@return table
function copytable(origtable) end
