-- hudprobe v7 : is the HUD staging buffer actually being filled?
--
-- Same deterministic scenario as v6 (load slot 2 at frame 3, START at 60 and
-- 130), plus a Lua write hook on the staging buffer 0x02100358..0x02100557 so
-- we can see whether the 1-word DMA fills really land there, and binary dumps
-- of that buffer and of the 8KB tile sheet it is filled from (0x022CE9xx).
--
-- Run once with JIT block size 16 and once with 15; the log / dumps are
-- timestamped so they do not overwrite each other.

local OUT = os.getenv("HUD_OUT") or "C:/Users/zhang/desmume/.workbuddy/scratch/"
if OUT:sub(-1) ~= "/" then OUT = OUT .. "/" end
local TS   = tostring(os.date("%m%d_%H%M%S"))
local LOG  = io.open(OUT .. "hud7_" .. TS .. ".log", "w")

local frame = 0
local function w(s) LOG:write(s); LOG:write("\n"); end
w("HUD probe v7 start " .. TS)

-- ------------------------------------------------------------------ buffers
local STAGE, STAGE_N = 0x02100358, 0x200
local SHEET, SHEET_N = 0x022CE800, 0x2200

local function dumpbin(tag, base, size)
  local t = memory.readbyterange(base, size)
  local f = io.open(OUT .. tag .. "_" .. TS .. ".bin", "wb")
  if not f then return end
  local buf = {}
  local i
  for i = 1, size do
    buf[#buf + 1] = string.char(t[i] or 0)
    if #buf >= 4096 then f:write(table.concat(buf)); buf = {} end
  end
  if #buf > 0 then f:write(table.concat(buf)) end
  f:close()
end

-- ------------------------------------------------------- staging write hook
local nw = 0
local function on_stage(addr, size)
  if nw >= 6000 then return end
  local v
  if size == 4 then
    v = memory.readword(addr)
  elseif size == 2 then
    v = memory.readword(addr) % 0x10000
  else
    v = memory.readbyte(addr)
  end
  nw = nw + 1
  w(string.format("W f=%d a=%08X s=%d v=%08X", frame, addr, size, v))
end

local ok, err = pcall(function() memory.registerwrite(STAGE, STAGE_N, on_stage) end)
w("HOOK register ok=" .. tostring(ok) .. " err=" .. tostring(err))

dumpbin("stage_f0", STAGE, STAGE_N)

-- ------------------------------------------------------------- scenario run
while true do
  if frame == 3 then
    w("MARK load-slot-2")
    local ok2, err2 = pcall(function() savestate.load(2) end)
    w("MARK load-result ok=" .. tostring(ok2) .. " err=" .. tostring(err2))
  elseif frame == 10 then
    dumpbin("stage_load", STAGE, STAGE_N)
  elseif frame == 59 then
    dumpbin("stage_premenu", STAGE, STAGE_N)
    dumpbin("sheet_premenu", SHEET, SHEET_N)
    w("MARK fills-so-far=" .. nw)
  elseif frame == 60 then
    w("MARK open-menu")
    joypad.set({start = true})
  elseif frame == 63 then
    joypad.set({start = false})
  elseif frame == 130 then
    w("MARK close-menu")
    joypad.set({start = true})
  elseif frame == 133 then
    joypad.set({start = false})
  elseif frame == 145 then
    dumpbin("stage_closed", STAGE, STAGE_N)
    dumpbin("sheet_closed", SHEET, SHEET_N)
    w("MARK fills-after-close=" .. nw)
  elseif frame == 200 then
    w("MARK end fills=" .. nw)
    LOG:flush()
    LOG:close()
    print("hudprobe v7 done, fills=" .. nw)
    return
  end
  frame = frame + 1
  emu.frameadvance()
end
