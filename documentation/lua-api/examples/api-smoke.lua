--[[----------------------------------------------------------------------------
	Smoke test for the scripting functions that were added to the Cocoa front end
	to reach parity with Windows.

	Companion to osd-shapes.lua, which covers the agg.* drawing layer; this one
	covers the frontend hooks: dialogs, menus, host input, layer masks and the
	status message.  Load it from the Lua Script window ("Run"), or with
	emu.openscript("api-smoke.lua").

	What to look for:

	  * the HUD reports the keys you are holding and the pointer position.  Move
	    the mouse over the touch screen (or the DS window in general) and
	    "pointer" should track it; on macOS y is negative over the upper screen,
	    exactly as Windows reports it.
	  * plug in a gamepad and "pad 0" switches from "no controller" to a button
	    and axis count.  controller.get() goes through IOHIDManager on macOS, so
	    a pad that only shows up to GameController.framework is not seen.
	  * "layer mask" cycles through five presets.  This changes the emulated
	    display itself, so the game screen behind the HUD goes dark or loses
	    layers as it runs - that is the test, not a bug.  Set CYCLE_LAYER_MASKS
	    to false to leave the display alone and use the menu instead.
	  * every two seconds emu.message() pushes a line into the OSD (macOS) or
	    the status bar (Windows).
	  * Tools > Lua API Test has four entries.  The first two open modal dialogs
	    and print the answer to the script console; the third exercises
	    emu.openscript() with a file that does not exist, which must raise a
	    catchable Lua error rather than abort the script; the fourth opens
	    osd-shapes.lua, whose HUD will fight with this one until you stop one of
	    the two scripts.

	emu.registermenustart() runs when the menu is about to open (Windows'
	WM_INITMENUPOPUP).  It is used below to tick the menu entry matching the
	layer mask that is currently active, which is what emu.setmenuiteminfo() is
	for.

	Stopping the script restores all display layers.

	This HUD deliberately polls input.get() and controller.get() on every frame
	as a load test.  Both are non-blocking on macOS (a cached pointer position
	and a mutex-protected HID snapshot), so the game should stay at full speed.
	Menu clicks and emu.registermenustart() are queued from the AppKit thread
	and run at the next frame boundary, because the Lua state belongs to the
	emulation thread; the pointer position can lag by one frame for the same
	reason.  emu.frameadvance() paces itself against an absolute frame deadline,
	so even a script that only calls frameadvance() does not add latency to each
	frame - the emulator should hold 60 fps with or without a script running.
------------------------------------------------------------------------------]]

-- Change to false to leave the emulated layers alone; the menu still works.
local CYCLE_LAYER_MASKS = true
local MASK_HOLD_FRAMES   = 90      -- 1.5 s per preset at 60 fps
local MESSAGE_INTERVAL   = 120     -- frames between emu.message() calls

local function rgb(r, g, b, a)
	return r * 0x1000000 + g * 0x10000 + b * 0x100 + (a or 255)
end

local COL_WHITE   = rgb(255, 255, 255)
local COL_DIM     = rgb(150, 150, 150)
local COL_YELLOW  = rgb(255, 214, 64)
local COL_GREEN   = rgb(64, 200, 120)
local COL_CYAN    = rgb(80, 220, 240)
local COL_ORANGE  = rgb(255, 140, 40)
local COL_MAGENTA = rgb(220, 80, 200)
local COL_PANEL   = rgb(0, 0, 0, 150)

-- Bit 0-3 are BG0-BG3, bit 4 is the object layer.
local LAYER_PRESETS = {
	{ name = "all on",   main = 0x1F, sub = 0x1F },
	{ name = "BG only",  main = 0x0F, sub = 0x0F },
	{ name = "OBJ only", main = 0x10, sub = 0x10 },
	{ name = "none",     main = 0x00, sub = 0x00 },
	{ name = "all on",   main = 0x1F, sub = 0x1F },
}

local presetIndex = 1
local lastMessageFrame = -MESSAGE_INTERVAL
local messageCount = 0
local lastMaskName = "?"

local function applyPreset(index)
	presetIndex = ((index - 1) % #LAYER_PRESETS) + 1
	local preset = LAYER_PRESETS[presetIndex]
	gui.setlayermask(preset.main, preset.sub)
	lastMaskName = preset.name
end

------------------------------------------------------------------------------
-- Menu
--
-- addmenu() takes an array, not a dictionary: every element is either a table
-- { label, handler } / { label, subEntries } or a nil separator.  The handlers
-- are named so that setmenuiteminfo() can identify them later.
------------------------------------------------------------------------------

local function menuGuiPopup()
	local answer = gui.popup(
		"gui.popup() test.\n\n" ..
		"This is a real native dialog, and the button order matches the Win32 " ..
		"MessageBox so scripts answer the same way on both platforms.",
		"okcancel", "question")
	print(string.format("gui.popup -> %s", tostring(answer)))
end

local function menuInputPopup()
	local answer = input.popup(
		"input.popup() test.\n\n" ..
		"Three buttons this time: Yes, No, Cancel.",
		"yesnocancel", "warning")
	print(string.format("input.popup -> %s", tostring(answer)))
end

local function menuMissingScript()
	-- emu.openscript() raises a Lua error on a file it cannot open.  Catching it
	-- here is the point: an uncaught one would stop the script.
	local ok, err = pcall(emu.openscript, "definitely-not-here.lua")
	print(string.format("emu.openscript(missing) -> ok=%s err=%s",
		tostring(ok), tostring(err)))
end

local function menuOpenSibling()
	-- Relative paths resolve next to the running script first, so this reaches
	-- the example that ships beside this one.  Guarded, because the two files
	-- may have been copied apart.
	local ok, err = pcall(emu.openscript, "osd-shapes.lua")
	if ok then
		print("emu.openscript(osd-shapes.lua) - expect a second HUD to appear")
	else
		print(string.format("emu.openscript(osd-shapes.lua) -> %s", tostring(err)))
	end
end

local function menuMaskAllOn()    gui.setlayermask(0x1F, 0x1F) end
local function menuMaskBgOnly()   gui.setlayermask(0x0F, 0x0F) end
local function menuMaskObjOnly()  gui.setlayermask(0x10, 0x10) end
local function menuMaskNone()     gui.setlayermask(0x00, 0x00) end

emu.addmenu("Lua API Test", {
	{ "gui.popup()...", menuGuiPopup },
	{ "input.popup()...", menuInputPopup },
	{ "emu.openscript() with a missing file", menuMissingScript },
	{ "emu.openscript(\"osd-shapes.lua\")", menuOpenSibling },
	{ "Layer masks", {
		{ "All layers on", menuMaskAllOn },
		{ "Backgrounds only", menuMaskBgOnly },
		{ "Objects only", menuMaskObjOnly },
		{ "Everything off", menuMaskNone },
	} },
})

-- Runs on every menu open; keeps the tick marks in step with the actual state.
emu.registermenustart(function()
	local active = { menuMaskAllOn, menuMaskBgOnly, menuMaskObjOnly, menuMaskNone }
	for i = 1, #active do
		emu.setmenuiteminfo(active[i], {
			checked = (i == presetIndex),
			enabled = true,
		})
	end
end)

------------------------------------------------------------------------------
-- HUD
------------------------------------------------------------------------------

local function sortedPressedKeys(state)
	local keys = {}
	for name, value in pairs(state) do
		if value == true and name ~= "xmouse" and name ~= "ymouse" then
			keys[#keys + 1] = tostring(name)
		end
	end
	table.sort(keys)
	return keys
end

local function joinFirst(keys, limit)
	if #keys == 0 then
		return "(nothing held)"
	end
	local shown = {}
	for i = 1, math.min(#keys, limit) do
		shown[i] = keys[i]
	end
	local text = table.concat(shown, " ")
	if #keys > limit then
		text = text .. string.format(" +%d", #keys - limit)
	end
	return text
end

local function drawLabel(y, text)
	agg.setFont("gse5x7")
	agg.lineColor(COL_DIM)
	agg.text(8, y, text)
end

local function hud()
	local frame = emu.framecount()

	if CYCLE_LAYER_MASKS and (frame % MASK_HOLD_FRAMES) == 0 then
		applyPreset(presetIndex + 1)
	end

	-- Background panel so the HUD stays readable over a bright game screen.
	agg.fillColor(COL_PANEL)
	agg.noLine()
	agg.rectangle(0, 0, 255, 118)

	agg.noFill()
	agg.lineColor(COL_WHITE)
	agg.lineWidth(1)
	agg.rectangle(0, 0, 255, 118)

	agg.setFont("verdana16_bold")
	agg.lineColor(COL_WHITE)
	agg.text(8, 16, "Lua API test")

	agg.setFont("gse5x7")
	agg.lineColor(COL_CYAN)
	agg.text(112, 16, string.format("f%d", frame))

	-- input.get()
	local state = input.get()
	local keys = sortedPressedKeys(state)
	drawLabel(30, "input.get()")
	agg.lineColor(keys == nil and COL_DIM or COL_YELLOW)
	agg.text(8, 39, joinFirst(keys, 8))

	if state.xmouse and state.ymouse then
		agg.lineColor(COL_WHITE)
		agg.text(8, 48, string.format("pointer %d, %d (touch-screen px)",
			state.xmouse, state.ymouse))
	else
		agg.lineColor(COL_DIM)
		agg.text(8, 48, "pointer (outside the display)")
	end

	-- controller.get()
	drawLabel(62, "controller.get()")
	local pad = controller.get(0, true)
	local padText
	if pad == nil or next(pad) == nil then
		padText = "pad 0: no controller"
		agg.lineColor(COL_DIM)
	else
		local buttons = 0
		local axes = 0
		local dpad = {}
		for name, value in pairs(pad) do
			if name == "up" or name == "down" or name == "left" or name == "right" then
				if value then dpad[#dpad + 1] = name end
			elseif type(value) == "boolean" then
				buttons = buttons + 1
			elseif type(value) == "number" then
				axes = axes + 1
			end
		end
		padText = string.format("pad 0: %d btn %d axes", buttons, axes)
		if #dpad > 0 then
			padText = padText .. " " .. table.concat(dpad, "+")
		end
		agg.lineColor(COL_GREEN)
	end
	agg.text(8, 71, padText)

	-- gui.setlayermask()
	drawLabel(85, "gui.setlayermask()")
	agg.lineColor(COL_ORANGE)
	local preset = LAYER_PRESETS[presetIndex]
	agg.text(8, 94, string.format("%s  main=0x%02X sub=0x%02X",
		lastMaskName, preset.main, preset.sub))

	-- emu.message()
	drawLabel(108, "emu.message()")
	agg.lineColor(COL_MAGENTA)
	agg.text(80, 108, string.format("sent %d", messageCount))

	-- Footer
	agg.setFont("gse5x7")
	agg.lineColor(COL_DIM)
	agg.text(8, 132, "Tools > Lua API Test")
	agg.lineColor(COL_WHITE)
	agg.noFill()
	agg.lineWidth(1)
end

gui.register(hud)

emu.registerexit(function()
	gui.setlayermask(0x1F, 0x1F)
	print("Lua API test: layers restored")
end)

applyPreset(1)

while true do
	local frame = emu.framecount()
	if frame - lastMessageFrame >= MESSAGE_INTERVAL then
		lastMessageFrame = frame
		messageCount = messageCount + 1
		emu.message(string.format("emu.message() #%d", messageCount))
	end
	emu.frameadvance()
end
