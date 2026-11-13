# DeSmuME Fork — Feature Overview

What this fork adds over upstream `origin/master`, grouped by platform, then by
feature. Each entry says what you can do with it — not how it is built.

---

## Shared / cross-platform

### WebGPU 3D renderer
- **A third 3D engine** next to OpenGL and the software rasterizer. Backends
  (D3D12, Vulkan, Metal, …) are detected at startup and can be chosen per
  machine.
- **Same picture as OpenGL** — per-polygon viewports, toon/shadow/decal, fog,
  edge marking, alpha and depth-equal tests, W-buffering, zero-alpha blending,
  4× MSAA and 16× anisotropy; every accuracy option behaves as it does under
  OpenGL.
- **Stays fast with texture upscaling** — one upload per frame and a texture
  cache that survives VRAM reconfiguration keep upscaled games at full speed.

### WebGPU display output and GPU filters
- **One presentation path for all frontends** — every frontend can offer WebGPU
  as a display method.
- **Shader-based upscaling** — a dozen libretro filter chains (xBRZ, Lanczos3,
  Bicubic, xBR, ScaleFX, super-xBR, …) run as multi-pass filters; only the
  selected chain is compiled and unsupported cases fall back to a plain blit.

### On-screen display and Lua HUD
- **GPU-rendered HUD** — OSD text and Lua drawing primitives are drawn on the
  GPU, so the HUD looks and behaves the same on Windows, macOS and Linux.
- **Consistent scripting surface** — shapes, gradients, text and the 34 bundled
  bitmap fonts behave identically everywhere, with CPU fallbacks where needed.

### Lua scripting engine
- **Runs on Lua 5.1 or LuaJIT 2.1** — both engines are supported, and trace JIT
  is off by default so scripts stay stoppable.
- **No more stop-script crashes** — scripts, menu callbacks and teardown belong
  to the emulation thread, which removes the UI-thread races behind them.
- **Documented API** — type definitions for all 158 registered names, a
  completeness check, and smoke-test / macro / OSD examples for TAS authors.

### Image scaling, audio and builds
- **xBRZ 1.9** — 2×–6× texture upscaling, faster than before and without the
  old 64 MB lookup table.
- **SDL3** — audio, joystick, microphone and window handling on every platform,
  including CI.
- **Audio and correctness** — SoundTouch 2.4.1 with an ARM64 fallback, correct
  red/blue order in the ARM NEON colour conversion, and SPU sync options
  labelled by behaviour instead of cryptic N/Z/P codes.

### RAM Watch and RAM Search: one shared core
- **One implementation, not one per frontend** — watch lists, memory access,
  value formatting, `.wch` files, recent lists and the search engine are shared;
  Windows and macOS behave identically.
- **Watch lists travel between platforms** — `.wch` files written on one
  frontend open unchanged on the other, with 1/2/4-byte widths, signed,
  unsigned, hexadecimal and 20.12 fixed-point values, separator rows and
  per-watch notes.
- **The two tools share their plumbing** — address parsing, bus reads and pokes,
  8/16/32-bit formatting and Add-to-Watch are defined once, so results look the
  same everywhere.
- **Preferences stay host-native** — Auto-load, window position and recent files
  are common state, saved by each frontend in its own way.

---

## macOS (Cocoa)

### Video recording
- **Hardware encoding** — encoders are listed as they are found (H.264, HEVC,
  AV1 and others where available), with MP4/MOV containers and a choice of
  audio codec.
- **What you see is what you record** — rotation, hybrid/16:9 layouts, view
  scale, output filters, pixel scalers and deposterize match the live view, and
  dropped frames keep audio and video aligned.
- **Light on memory** — frames go straight from the GPU into the encoder, so
  there is no conversion thread and no multi-gigabyte staging buffer. Recording
  needs the Metal display path; screenshots still work on OpenGL.

### Lua scripting console
- **Native console** with script run/stop/edit, menus, on-screen messages,
  popups and host input, closing the gap with Windows.
- **No Lua install required** — the engine is loaded at runtime and the app
  starts even without it.
- **Correct frame pacing** — scripted `frameadvance()` holds 60 FPS, honours
  speed settings and never blocks on input polling.

### Memory Viewer
- **Live hex dump of every region** — ARM9, ARM7, firmware, cartridge ROM and
  the whole address space, as bytes, halfwords or words, with ASCII, optional
  big-endian reads and a Q16.16 read-out in word view. Only visible rows are
  fetched, so even the 4 GB address space opens instantly.
- **Editing in place** — type hexadecimal digits into a cell to patch the
  emulated value, then keep moving with the arrow keys, Page Up/Down and
  Home/End; a scroller and trackpad pan the dump, and recent addresses stay
  available.
- **Breakpoints and export** — mark an address to stop the emulator on the next
  access (the byte turns red); dump the visible window as text or raw bytes, or
  a whole region as a binary file.

### RAM Watch
- **A live watch list** — Address / Value / Notes refresh every frame, with
  1/2/4-byte values as signed, unsigned, hexadecimal or 20.12 fixed point. Any
  address can be watched, not only the regions RAM Search scans.
- **Editing and organisation** — add, edit, duplicate, remove, reorder and
  group with separator rows; duplicates are rejected, the list is capped at 256,
  and unsaved changes are confirmed before closing.
- **Files and persistence** — open, save, save-as and append Windows-compatible
  `.wch` lists, a recent-files menu, Auto-load on launch, remembered window
  position, a default name taken from the loaded ROM, and drag-and-drop.
- **Tools > RAM Watch… (Cmd+Opt+W)** — a RAM Search result can be added straight
  to the list.

### RAM Search
- **Find unknown values by how they change** — scan main RAM, ITCM and DTCM and
  narrow candidates by specific value, change since last frame, specific
  address or number of changes.
- **The full comparison set** — equal, not equal, less, greater, ≤, ≥,
  Different By and Modulo Is, with 1/2/4-byte widths, signed/unsigned/hex/20.12
  display, optional misaligned addresses and a big-endian toggle.
- **The Windows dialog layout** — the result list and its action column on top,
  the four criteria groups (Comparison Operator, Compare To / By, Data Size,
  Data Type / Display) underneath, so every option is visible at once.
- **Narrow down without starting over** — Undo and Redo step through searches,
  Eliminate drops a wrong candidate, Clear Change Counts restarts the change
  counters, and Autosearch keeps applying the current criteria as the game runs
  (Retry / Ignore / Stop when the list runs dry).
- **Poke and hand off** — Poke writes the Specific Value into the selected
  address, while Watch and Add Cheat turn a row into a RAM Watch entry or an
  internal cheat, opening the target window when it is closed.
- **A readable live result table** — Address / Value / Previous / Changes,
  refreshed every frame and throttled for very large sets; double-click a row to
  send it to RAM Watch.
- **Tools > RAM Search… (Cmd+Opt+S)**

### Metal OSD for Lua and the HUD
- **AGG-free rendering** — Lua drawing and OSD text go into a HUD layer shared by
  the live view, screenshots and recording, so all three look the same.

### WebGPU 3D renderer
- **Bundled and signed for both architectures** — WebGPU appears as a 3D engine
  choice in the Universal build, using the Metal backend.

---

## Windows

### WebGPU renderer and display method
- **Per-backend selection** — the display method and 3D engine lists each offer
  one entry per usable backend; the choice is remembered and falls back safely.
- **Safe switching** — a WebGPU switch on a window OpenGL already drew to is
  refused with an explanation instead of crashing; restarting picks it up.

### MP4 video recording
- **Modern codecs** — MP4 with H.264, HEVC or AV1 video and AAC, MP3, FLAC,
  ALAC, Opus or AC-3 audio; hardware encoders appear when present, unavailable
  ones never do.
- **Composited output** — LCD layout, screen gap, rotation and CPU video filters
  at 1×–4× are baked into the recording. Legacy AVI/WAV recorders moved under
  "Legacy Recording".

### GPU output filters
- **WGSL shader chains** are selectable from a "GPU Filter" menu on the WebGPU
  display method, and the choice is restored when switching back.

### Movie dialogs (Play Movie / Record Movie)
- **The whole movie is previewed before it starts** — picking a file fills in its
  ROM (serial, CRC and file name) beside the ROM that is loaded, plus author,
  comment, record date, core version, length, frame count and rerecord count.
  OK stays disabled until the file can be read.
- **Mismatches stand out** — a movie recorded against another ROM or another core
  version is highlighted in red, and the core line says "desync expected".
- **Pause at a frame** — playback stops automatically at a chosen frame
  (default: the last one).
- **Comments are editable** — with the movie opened read-write, the comment can
  be changed before playing; only the comment lines of the header are rewritten
  and the recorded input is left untouched. Read-only movies lock the field.
- **Three recording start points** — power on, reset/SRAM (with a `.dsv`
  picker) or the current emulator state, each with the ROM and the emulation
  settings the movie will store shown alongside.
- **File names that make sense** — both dialogs show only the file name and
  resolve it against Path Settings > Movies; Browse opens in that folder, and
  recording never overwrites an existing movie (`ROM.dsm` → `ROM - 1.dsm`).

### Movies directory
- **Path Settings > Movies** — movies get their own configurable folder next to
  screenshots, states and AVI files; it defaults to `.\Movies` and is created on
  demand.

### Movie metadata: record date and comments
- **Recording time is stored** — the wall-clock time a movie was recorded is
  kept in the file and shown in the Play dialog. Movies from older builds simply
  report "Unknown", and older builds ignore the field.
- **Free-form comments** — a description typed at record time is stored as an
  ordinary comment, so it opens in any DSM-aware tool and can be edited later.

### DSM Editor (movie timeline editor)
- **The recorded input as a frame grid** — one row per frame, one column per
  input the movie carries: the 13 pad bits, stylus (x,y), microphone, lid and,
  when present, the reset command. A three-hour movie stays as responsive as a
  three-second one.
- **Edit the timeline, not just watch it** — click a cell to toggle that input on
  that frame, double-click the stylus cell to type a position, and insert,
  delete, cut, copy or paste whole frame ranges. Marks flag frames and jump
  between them. The window opens read-only, so looking cannot change anything.
- **A plain CSV on the clipboard** — a block of frames copies out as
  `frame,R,L,D,U,T,S,B,A,Y,X,W,E,G,TouchX,TouchY,Touch,Mic,Lid,Reset`, editable
  in Notepad and pasted back; a rejected paste names the line it could not read.
- **Save writes the .dsm back** — the whole movie is re-dumped in the format it
  was loaded with (text or binary), and the title marks unsaved edits.

### Modern UI font and per-monitor DPI
- **Crisp at any scaling** — window presets, toolbar, debug viewers, list columns
  and sound bars scale with the monitor instead of being bitmap-stretched, and
  window sizes persist in DPI-independent units.
- **System UI font** — dialogs use Segoe UI / Microsoft YaHei UI, with a
  fixed-pitch face for disassembler and hex views and a "Use Modern UI Font"
  toggle, so text stays legible at 200%.

### Screenshots
- **Any format the system supports** — encoders are detected at runtime, with
  bundled PNG/BMP writers as fallback, so the executable still starts on old
  systems.

### OSD and Lua runtime
- **No AGG dependency** — OSD text uses embedded bitmap fonts, and a WebGPU OSD
  backend serves the default build.
- **LuaJIT 2.1** ships with the executable; "About Lua Scripting" reports the
  running engine, JIT policy and headers under Tools.

### RAM Watch on the shared core
- **Same behaviour, one codebase** — the dialog is a thin host over the shared
  core, so `.wch` files, formatting, watch limits and recent lists match macOS
  exactly.
- **Fixed along the way** — no more fixed-size arrays or leaked comment strings,
  no inverted window-position check, no value-column overflow, no scratch row
  overrunning the list, no stale bytes left after saving, and poking a RAM
  Search result now actually writes to emulated memory.

### Build and toolchain
- **Current dependencies** — system DirectX libraries instead of a legacy
  prebuilt blob, a vendored SDL3 SDK, no unused SDL 1.2 headers, and clean x64
  builds (including a real Lua console bug).

---

## Linux / POSIX (GTK3)

### WebGPU presentation (X11 and Wayland)
- **Native presentation** in the GTK drawing area, with Wayland avoiding races
  against GTK's own frames; unsupported layouts or devices fall back to software
  rendering.
- **Runtime choice** — a "Screen presentation" option switches between WebGPU
  and Cairo, one entry per usable backend, and closing the emulator no longer
  crashes.

### GPU output filters and WebGPU HUD
- **Shared filters** are offered in a "GPU filter" combo in Graphics settings,
  greyed out during software presentation.
- **GPU-composited HUD** — OSD and Lua drawing is layered over the frame, and
  screenshots include it, with a CPU fallback.

### Lua scripting
- **No development headers needed** — LuaJIT or Lua 5.1 is loaded at runtime;
  the console offers Run/Stop/Edit, file watching, drag-and-drop, an About
  dialog, frame stepping, popups, menus and host input APIs.

### Input, audio and build simplification
- **SDL-based microphone** replaces the optional ALSA/OpenAL backends, removing
  two external dependencies; SDL3 then covers audio, gamepad and window handling
  for every POSIX frontend.
- **Leaner tree** — the retired GTK2 frontend is gone from Meson, autotools,
  Code::Blocks and CI, AGG is built in-tree, and the bundled wgpu-native library
  is found on both x86-64 and ARM64.
- **HUD layout** saves and restores correctly even in builds without AGG.
