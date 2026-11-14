--[[==========================================================================
  macro.lua -- hold a trigger key and the emulator replays a button sequence

  Port of vba-rr-modernized/documentation/lua-examples/macro.lua.  The
  behaviour is the same; only the emulator-facing parts differ.

  Behaviour
    press it      : start from step 1, wrap back to step 1 after the last step,
                    loop forever
    keep holding  : keep looping
    release it    : stop on that very frame, the pad is handed straight back
    press again   : start over from step 1 (never resumes where it stopped)

  Usage
    DeSmuME -> Tools -> Lua Scripting -> New Lua Script Window
    -> Open... pick this file -> Run

  ---------------------------------------------------------------------------
  What changed from the VBA-RR original

  * Buttons.  The DS has A B X Y L R start select up down, so two more face
    buttons than the GBA.  The character map below keeps the original
    mnemonics - "<" ">" "^" "v" for the d-pad, S = start, E = select, any other
    capital letter is the face button with that name - and adds X and Y.

  * joypad.set(0, pad) exists here as well, but it does not replace the pad
    the way VBA-RR's does: it only writes the keys that are present in the
    table and leaves the rest alone (lua-engine.cpp, joy_set: "nil means no
    change").  While a macro is running this script therefore writes every
    button explicitly - see MACRO_OWNS_PAD - so the macro really takes the pad
    over instead of OR-ing with whatever the player happens to be holding.

  * Timing.  emu.registerbefore() is DeSmuME's registerbefore: it runs between
    NDS_beginProcessingInput() and NDS_endProcessingInput(), i.e. after the
    host input was sampled but before the game latches it, and that is exactly
    where joypad.set() is honoured instead of being deferred to the next frame
    (NDSSystem.cpp, NDS_beginProcessingInput).  gui.register() is the fallback;
    it hangs off the rendered frame and can be skipped while frame-skipping,
    which makes a macro stutter.

  * Lag frames.  emu.lagged() tells whether the last frame did not read the
    input, the same idea as VBA-RR's emu.lagged(), so SKIP_LAG still works.

  * Life cycle.  A DeSmuME script is stopped the moment its main chunk returns,
    so this one keeps itself alive with the usual
    "while true do emu.frameadvance() end" loop; the per-frame callback keeps
    firing from there.

  * joypad.set() is ignored while a movie is playing, and input.get() only
    reports keys while the emulator window has the focus.  Both are inherited
    from the emulator, not from this script.
==========================================================================]]

------------------------------------------------------------------- config --
-- One entry per binding. Every macro keeps its own step counter, so several
-- triggers may be held at once: their buttons are OR-ed into a single
-- joypad.set call. Binding the same key twice is a hard error.
--
--   key      : trigger key, named the way input.get() names it:
--              "A".."Z", "0".."9", "space", "enter", "shift", ...
--   sequence : ordered array of steps.
--       keys   : buttons held during the step. "" means nothing.
--                A B X Y L R face buttons (upper case), < > ^ v directions,
--                S = start, E = select.
--                Characters may be concatenated, e.g. ">vA" = right+down+A.
--       frames : how many frames the step lasts
local MACROS = {
  {
    key = "M",
    sequence = { -- AoS
      { keys = "A",   frames = 1 },   -- A            1 frame
      {               frames = 1 },   -- nothing      1 frame
      { keys = "A",   frames = 1 },   -- A            1 frame
      {               frames = 1 },   -- nothing      1 frame
      { keys = ">vA", frames = 1 },   -- right+down+A 1 frame
      {               frames = 1 },   -- nothing      1 frame
    },
  },

  {
    key = "N",
    sequence = { -- AoS
      { keys = "A",   frames = 1 },   -- A            1 frame
      {               frames = 1 },   -- nothing      1 frame
      { keys = "A",   frames = 1 },   -- A            1 frame
      {               frames = 1 },   -- nothing      1 frame
      { keys = "<vA", frames = 1 },   -- left+down+A  1 frame
      {               frames = 1 },   -- nothing      1 frame
    },
  },
}

-- While a macro is held, clear the buttons the step does not use, so the
-- macro owns the pad completely (what VBA-RR's joypad.set did by design).
-- Set to false to let your own presses through and only add the macro's.
local MACRO_OWNS_PAD = true

-- lag frames (frames on which the game did not read the input) do not consume
-- macro time
local SKIP_LAG = true

-- print "start / stop" lines to the Lua console
local VERBOSE = true

-- print one line per step change (noisy for very short steps, off by default)
local DEBUG_STEPS = false

------------------------------------------------------------- implementation --
local ext    = rawget(_G, "emu") or {}
local lagged = ext.lagged          -- emu.lagged(): did the last frame skip input?
local joypadSet = rawget(_G, "joypad") and joypad.set
if type(joypadSet) ~= "function" then
  error("macro.lua: this emulator has no joypad.set()", 0)
end

-- character -> button_mappings[] name (lua-engine.cpp, button_mappings; the
-- case matters: the face buttons are upper case, the rest are not)
local CHAR2BUTTON = {
  ["A"] = "A",      ["B"] = "B",      ["X"] = "X",       ["Y"] = "Y",
  ["L"] = "L",      ["R"] = "R",
  ["<"] = "left",   [">"] = "right",  ["^"] = "up",      ["v"] = "down",
  ["V"] = "down",
  ["S"] = "start",  ["E"] = "select",
}

-- cross-check: these twelve names are all the buttons a macro may use
local KNOWN = {
  A = true, B = true, X = true, Y = true, L = true, R = true,
  start = true, select = true,
  up = true, down = true, left = true, right = true,
}

-- ">vA" -> { A=true, right=true, down=true }
local function parseKeys(spec)
  local pad = {}
  if spec == nil then
    return pad
  end
  for ch in tostring(spec):gmatch(".") do
    local name = CHAR2BUTTON[ch]
    if name then
      pad[name] = true
    elseif ch ~= "+" and ch ~= " " then
      -- "+" and " " are plain separators, so ">+v+A" parses as well
      error(("macro.lua: bad key char '%s' in \"%s\""):format(ch, tostring(spec)), 0)
    end
  end
  return pad
end

-- Pre-process one MACROS entry: compile the keys strings into tables that can
-- be handed to joypad.set as they are, and give the macro its own runtime state.
local function compileMacro(entry, pos)
  if type(entry) ~= "table" then
    error(("macro.lua: MACROS[%d] is not a table"):format(pos), 0)
  end
  local key = entry.key
  if type(key) ~= "string" or key == "" then
    error(("macro.lua: MACROS[%d].key must be a non-empty string"):format(pos), 0)
  end
  local seq = entry.sequence
  if type(seq) ~= "table" then
    error(("macro.lua: MACROS[%d] (key %q) has no sequence table"):format(pos, key), 0)
  end

  local steps = {}
  for i = 1, #seq do
    local src = seq[i]
    if type(src) ~= "table" then
      error(("macro.lua: MACROS[%d].sequence[%d] is not a table"):format(pos, i), 0)
    end
    local n = math.floor(tonumber(src.frames) or 0)
    if n < 1 then
      error(("macro.lua: MACROS[%d].sequence[%d].frames must be >= 1"):format(pos, i), 0)
    end
    local pad = parseKeys(src.keys)
    for name in pairs(pad) do
      if not KNOWN[name] then
        error(("macro.lua: unknown button %q"):format(name), 0)
      end
    end
    steps[i] = {
      pad    = pad,
      frames = n,
      label  = (src.keys ~= "") and src.keys or "-",
    }
  end
  if #steps == 0 then
    error(("macro.lua: MACROS[%d] (key %q) has an empty sequence"):format(pos, key), 0)
  end

  return {
    key    = key,
    steps  = steps,
    active = false,                -- is the trigger currently held?
    index  = 1,                    -- current step
    remain = steps[1].frames,      -- frames left on the current step
  }
end

local macros   = {}
local usedKeys = {}
for i = 1, #MACROS do
  local m = compileMacro(MACROS[i], i)
  if usedKeys[m.key] then
    error(("macro.lua: key %q is bound more than once"):format(m.key), 0)
  end
  usedKeys[m.key] = true
  macros[i] = m
end
if #macros == 0 then error("macro.lua: MACROS is empty", 0) end

-- One call for the whole pad.  With MACRO_OWNS_PAD the unused buttons are
-- written as false; joypad.set() treats an absent key as "leave it alone",
-- which on the DS would keep the player's own presses alive next to the macro.
local full = {}
local function sendPad(pad)
  if MACRO_OWNS_PAD then
    for name in pairs(KNOWN) do
      full[name] = pad[name] or false
    end
    joypadSet(0, full)
  else
    joypadSet(0, pad)
  end
end

local function tick()
  local keys  = input.get()
  local pad   = {}
  local anyOn = false

  for i = 1, #macros do
    local m    = macros[i]
    local held = (keys ~= nil) and (keys[m.key] ~= nil)

    -- ---- trigger not held: this macro outputs nothing ----
    -- Its bits simply never reach the merged pad, and when no macro is active
    -- joypad.set is not called at all, so the game gets the host's own input.
    if not held then
      if m.active then
        m.active = false
        if VERBOSE then
          print(("[macro] %s released -> stopped, reset to step 1"):format(m.key))
        end
      end
      -- rewind every idle frame, so the next press always starts at step 1
      m.index, m.remain = 1, m.steps[1].frames
    else
      -- ---- trigger just went down: start over ----
      if not m.active then
        m.active = true
        m.index, m.remain = 1, m.steps[1].frames
        if VERBOSE then
          print(("[macro] %s pressed -> start from step 1"):format(m.key))
        end
      end

      -- contribute the current step to the merged pad
      local step = m.steps[m.index]
      for name in pairs(step.pad) do pad[name] = true end
      anyOn = true

      -- ---- advance the timer; lag frames are free ----
      if not (SKIP_LAG and lagged ~= nil and lagged()) then
        m.remain = m.remain - 1
        if m.remain <= 0 then
          m.index = m.index + 1
          if m.index > #m.steps then m.index = 1 end
          m.remain = m.steps[m.index].frames
          if DEBUG_STEPS then
            print(("[macro] %s step %d/%d  %s x %d"):format(
              m.key, m.index, #m.steps,
              m.steps[m.index].label, m.steps[m.index].frames))
          end
        end
      end
    end
  end

  if anyOn then
    sendPad(pad)
  end
end

-- Prefer hooking in before the game latches the input (runs on every emulated
-- frame); only fall back to gui.register when that is unavailable.
local register = ext.registerbefore or (rawget(_G, "gui") and gui.register)
if type(register) ~= "function" then
  error("macro.lua: neither emu.registerbefore nor gui.register is available", 0)
end
register(tick)

print(("[macro] ready: %d binding(s), keys %s"):format(
  #macros, (function()
    local names = {}
    for i = 1, #macros do names[i] = macros[i].key end
    return table.concat(names, " ")
  end)()))

-- Keep the script (and therefore the callback above) alive; emu.frameadvance()
-- yields to the emulator and returns on the next frame boundary.
while true do
  emu.frameadvance()
end
