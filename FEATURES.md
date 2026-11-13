# DeSmuME Fork — Feature Overview

A compact summary of what this fork adds over upstream `origin/master`, grouped by
**platform → feature → highlights**. Each item describes what the feature does and
what it is useful for, without implementation detail.

---

## Shared / Cross-platform

### WebGPU 3D renderer (wgpu-native)
- **A third 3D engine** alongside OpenGL and the software rasterizer, built on
  wgpu-native v29; backends (D3D12, Vulkan, Metal, …) are discovered at runtime
  and selectable per machine.
- **Full NDS rendering fidelity** — per-polygon viewports, toon/shadow/decal,
  fog, edge marking, alpha and depth-equal tests, W-buffering, special zero-alpha
  blending, 4× MSAA and 16× anisotropy, so accuracy options behave as under OpenGL.
- **Faster by design** — one buffer upload per frame, GPU-side framebuffer
  conversion, persistent texture cache across VRAM reconfigures, and a
  non-spinning GPU wait; texture-upscaled games stay at full speed.

### WebGPU display output and GPU output filters
- **A shared presentation layer** (`wgpu_present`) composites the finished frame
  on the GPU, so all frontends can offer the same WebGPU display method.
- **Shader-based upscaling** — a dozen libretro WGSL chains (xBRZ, Lanczos3,
  Bicubic, xBR, ScaleFX, super-xBR, …) run as multi-pass filters; only the
  selected chain is compiled, and unsupported cases fall back to a plain blit.

### On-screen display / Lua HUD on the GPU
- **AGG-free OSD** — OSD text and `agg.*` Lua primitives are recorded and
  rasterized with SDF/glyph pipelines into a HUD texture instead of a CPU
  composite; the same backend serves Windows, macOS and Linux.
- **Consistent scripting surface** — shapes, gradients, text and the 34 bundled
  bitmap fonts behave the same everywhere, with CPU fallbacks where needed.

### Lua scripting engine
- **Portable engine core** — rewritten against the public Lua 5.1 C API so it
  builds with both vanilla Lua 5.1 and LuaJIT 2.1; trace JIT is off by default
  so scripts remain stoppable.
- **Safer lifecycle** — scripts, menu callbacks and teardown are owned by the
  emulation thread, removing UI-thread races and stop-script crashes.
- **Documented API** — LuaLS type definitions for all 158 registered names,
  a drift-check script, and smoke-test/macro/OSD examples for TAS authors.

### Image scaling, audio and build upgrades
- **xBRZ 1.9** — SIMD-based BT.2020 colour distances replace the old 64 MB
  lookup table; faster texture upscaling with 2×–6× filter support.
- **SDL3 migration** — audio, joystick, microphone and window backends plus all
  build systems and CI move from SDL2 to SDL3 for modern platform support.
- **Audio and correctness** — SoundTouch 2.4.1 (with an ARM64 Meson fallback),
  a fix for red/blue swap in the ARM NEON 555→888 conversion, and SPU sync
  options labelled by behaviour instead of cryptic N/Z/P codes.
- **Documentation and CI** — migration/OSD design notes, audio sync docs and
  refreshed CI runners.

### RAM Watch and RAM Search: one shared debugger core
- **One implementation, not one per frontend** — the watch list, emulated
  memory reads and writes, value formatting, `.wch` files, recent lists and
  the RAM Search engine live in a platform-independent core. Windows and
  macOS share the RAM Watch behaviour; Windows' RAM Search dialog still runs
  its legacy engine, but the shared one can replace it without a rewrite.
- **Watch lists travel between platforms** — `.wch` files written by either
  frontend open unchanged on the other, with 1/2/4-byte widths, signed,
  unsigned, hexadecimal and 20.12 fixed-point displays, separator rows and
  per-watch notes.
- **The two tools share their plumbing** — address parsing and validation,
  bus reads and poke writes, the 8/16/32-bit formatting rules and the
  Add-to-Watch action are defined once, so results look the same everywhere.
- **Preferences stay host-native** — Auto-load, window position and the
  recent-file list are held by the core and persisted by each frontend (INI
  on Windows, `NSUserDefaults` on macOS); a future GTK frontend can adopt the
  core without reimplementing any of it.

---

## macOS (Cocoa)

### Video recording (Metal + VideoToolbox)
- **Zero-copy GPU capture** — a dedicated Metal presenter renders each frame
  straight into an IOSurface-backed `CVPixelBuffer`; no texture readback, no
  conversion threads and no multi-gigabyte RGB staging buffer.
- **Hardware-accelerated encoding** — encoders are enumerated from VideoToolbox
  at runtime (H.264/HEVC, AV1 and others where available), with MP4/MOV
  container and audio codec selection.
- **What you see is what you record** — rotation, hybrid/16:9 layouts, view
  scale, output filters, pixel scalers and deposterize match the live view;
  dropped frames keep the audio/video timeline aligned. Requires the Metal
  display path; screenshots still work on OpenGL.

### Lua scripting console
- **Native AppKit console** with script run/stop/edit, menus, `emu.message`
  on-screen text, popups and host input, closing the feature gap with Windows.
- **LuaJIT loaded dynamically** via `dlopen` (Homebrew/MacPorts/bundle), so the
  app starts even without Lua installed and links against nothing Lua-related.
- **Correct frame pacing** — an accumulating deadline keeps scripted
  `frameadvance()` at 60 FPS, honours speed settings and avoids per-frame
  blocking on HID/input polling.

### Memory Viewer
- **Live hex dump of every memory region** — ARM9, ARM7, firmware, cartridge
  ROM and the full address space, read as bytes, halfwords or words with an
  ASCII column, optional big-endian interpretation and a Q16.16 read-out in
  word view. Only the rows on screen are fetched, so even the 4 GB address
  space opens instantly.
- **Editing in place** — click a cell and type hexadecimal digits to patch the
  emulated value, then keep moving with the arrow keys, Page Up/Down and
  Home/End; a scroller and trackpad scrolling pan the dump, and recently
  visited addresses are kept for reuse.
- **Read/write breakpoints and export** — marking an address stops the emulator
  the next time it is accessed and paints the byte red; the visible window can
  be dumped as text or raw bytes, or the whole region as a binary file.

### RAM Watch
- **A live watch list** — Address / Value / Notes refresh every frame, with
  1-, 2- or 4-byte values shown as signed, unsigned, hexadecimal or 20.12
  fixed point. Any address can be watched, not just the RAM regions that
  RAM Search scans.
- **Editing and organisation** — add, edit, duplicate, remove, reorder and
  group watches with separator rows from a vertical button bar; duplicate
  entries are rejected, the list is capped at 256, and unsaved changes are
  confirmed before the window closes.
- **Files, drag-and-drop and persistence** — open, save, save-as and append
  Windows-compatible `.wch` lists, a recent-files menu, Auto-load on launch,
  remembered window position, a default filename derived from the loaded ROM,
  and `.wch` files dropped onto the window.
- **Tools > RAM Watch… (Cmd+Opt+W)** — a search result can be added straight
  to the list from RAM Search.

### RAM Search
- **Find unknown values by how they change** — scan main RAM, ITCM and DTCM
  and narrow candidates with specific value, relative-to-previous, specific
  address and number-of-changes searches, backed by per-frame value snapshots
  and change counters.
- **The full comparison set** — equal, not equal, less, greater, ≤, ≥,
  Different By and Modulo Is, with 1/2/4-byte widths, signed, unsigned,
  hexadecimal or 20.12 display, optional misaligned addresses and a
  little-endian toggle.
- **A readable live result table** — Address / Value / Previous / Changes,
  refreshed every frame and automatically throttled for very large result
  sets; Reset starts over from all candidates, and double-clicking or
  Add Watch sends a row to the RAM Watch list.
- **Tools > RAM Search… (Cmd+Opt+S)** — this first pass covers core
  searching; Eliminate, Poke, Undo/Redo controls, Auto-search and Add Cheat
  are not exposed yet.

### Metal OSD for Lua and the HUD
- **AGG-free rendering** — MSL SDF/glyph pipelines draw `agg.*` primitives and
  OSD text into a HUD texture that the drawable, screenshot and recording paths
  all share; fonts are compile-time arrays because Mach-O lacks ELF sections.

### WebGPU 3D renderer
- **Universal wgpu-native dylib** (arm64 + x86_64) is bundled and signed with
  the app; macOS exposes the Metal backend, so WebGPU appears as a 3D engine
  choice in the Universal target.

---

## Windows

### WebGPU renderer and display method
- **Per-backend selection** — the display method and 3D engine lists each offer
  one entry per usable backend; the choice is persisted and falls back safely.
- **Safe switching** — the port refuses a WebGPU switch on a window OpenGL has
  already presented to (which would abort wgpu), explaining that a restart is
  needed rather than crashing.

### MP4 video recording (Media Foundation)
- **Modern codecs** — `IMFSinkWriter` writes MP4 with H.264, HEVC or AV1 video
  and AAC, MP3, FLAC, ALAC, Opus or AC-3 audio; encoders are enumerated from
  installed MFTs, so hardware encoders appear when present and unavailable ones
  never do.
- **Composited output** — LCD layout, screen gap, rotation and CPU video
  filters at 1×–4× are baked into the recording; legacy AVI/WAV recorders move
  under a "Legacy Recording" submenu.

### GPU output filters
- **WGSL shader chains** are exposed through a "GPU Filter" menu on the WebGPU
  display method; the selection is remembered and re-applies when switching
  back from another display method.

### Modern UI font and per-monitor DPI
- **PerMonitorV2 awareness** — window presets, toolbar, debug viewers, list
  columns and sound-level bars scale crisply with the monitor instead of being
  bitmap-stretched; window sizes persist in DPI-independent units.
- **System UI font** — dialogs use Segoe UI / Microsoft YaHei UI (and a
  TrueType fixed-pitch face for disassembler/hex views) with a "Use Modern UI
  Font" toggle, so text stays legible at 200% scaling.

### Screenshots (WIC)
- **Runtime-detected encoders** write any format Windows Imaging Component
  supports, with the bundled PNG/BMP writers as fallback; WIC stays a
  runtime-only dependency, so the executable still starts on old systems.

### Play Movie / Record Movie dialogs
- **The whole movie is previewed before it starts** — picking a file fills in its
  ROM (serial, CRC, file name) beside the ROM that is actually loaded, plus
  author, comment, record date, core version, length, frame count and rerecord
  count; OK stays disabled until the file can be read.
- **Mismatches are called out** — a movie recorded against another ROM or
  another core version is highlighted in red (and the core line says
  "desync expected"), so an obviously wrong movie is caught before playback.
- **Pause at a frame** — playback can stop automatically at a chosen frame
  (default: the last one) instead of running to the end.
- **Comments are editable** — with the movie opened read-write, the comment
  field can be changed before playing; only the comment lines of the header are
  rewritten and the recorded input is left untouched. Read-only movies lock the
  field.
- **Recording destinations and start points** — Record Movie offers power on,
  reset/SRAM (with a `.dsv` picker) or the current emulator state, shows the ROM
  and the emulation settings the movie will store, and takes an author and a
  comment.
- **File names that make sense** — both dialogs show only the file name and
  resolve it against Path Settings > Movies, Browse opens in that folder, and
  recording never overwrites an existing movie (`ROM.dsm` → `ROM - 1.dsm`).

### Movies directory
- **Path Settings > Movies** — recorded and played movies get their own
  configurable folder next to screenshots, states and AVI files, defaulting to
  `.\Movies` and created on demand.

### Movie metadata: record date and comments
- **Recording time is stored** — a new `recordDate` header field keeps the
  wall-clock time a movie was recorded; the Play dialog shows it, movies
  recorded by older builds simply report "Unknown", and older builds ignore the
  field when they load the file.
- **Free-form comments** — a description typed at record time is stored as an
  ordinary comment line, so it opens in any DSM-aware tool and can be edited
  later from the Play dialog.

### DSM Editor (movie timeline editor)
- **The recorded input as a frame grid** — one row per frame, one column per
  input the movie carries: the 13 pad bits, the stylus (x,y), the microphone
  button, the lid and, when present, the reset command. A virtual list keeps a
  three-hour movie as cheap as a three-second one.
- **Edit the timeline, not just watch it** — click a cell to toggle that input
  on that frame, double-click the stylus cell to type an exact position, and
  insert, delete, cut, copy or paste whole frame ranges. Marks (`M`) flag
  frames and the two mark buttons jump between them. The window opens
  read-only, so looking at a timeline cannot change it by accident.
- **A plain CSV on the clipboard** — a block of frames copies out as
  `frame,R,L,D,U,T,S,B,A,Y,X,W,E,G,TouchX,TouchY,Touch,Mic,Lid,Reset`, so it
  can be edited in Notepad and pasted back; a rejected paste names the line it
  could not read.
- **Save writes the .dsm back** — the whole movie is re-dumped in the format it
  was loaded with (text or binary), and the title marks unsaved edits. The
  emulator keeps exactly one playback path: the transport buttons forward the
  File menu's own Pause and Frame Advance.

### OSD and Lua runtime
- **GDI+ OSD** replaces libagg with embedded bitmap fonts, and an AGG-free
  WebGPU OSD backend keeps the default build free of the AGG library.
- **LuaJIT 2.1** ships as `lua51.dll`; "About Lua Scripting" reports the
  running engine, JIT policy, headers and DLL under Tools.

### RAM Watch on the shared core
- **Same behaviour, one codebase** — the Win32 dialog is now a thin host over
  the shared core, so `.wch` files, formatting, watch limits and recent lists
  match macOS exactly; RAM Search's bus reads, poke writes and address
  validation call the same low-level helpers.
- **Fixes found along the way** — fixed-size global arrays and malloc'd
  comment strings are gone, an inverted window-position check, a value-column
  buffer overflow and a scratch row that could overrun the list are corrected,
  saving a list no longer leaves stale bytes behind, and poking a RAM Search
  result now actually writes to emulated memory.

### Build and toolchain cleanup
- Links the **Windows SDK DirectX libraries** instead of a legacy prebuilt
  blob, vendors the **SDL3 3.4.20 SDK**, removes unused SDL 1.2 headers, and
  silences x64 pointer-truncation warnings (including a real Lua console bug).

---

## Linux / POSIX (GTK3)

### WebGPU presentation (X11 and Wayland)
- **Native presentation** into the GTK drawing area, using a child
  `wl_subsurface` on Wayland to avoid racing GTK's own Cairo frames;
  unsupported layouts or devices fall back to software Cairo.
- **Runtime choice** — a "Screen presentation" option switches between
  WebGPU and Cairo, with one entry per usable backend; the presentation
  instance is torn down before GTK releases EGL, avoiding an exit crash.

### GPU output filters and WebGPU HUD
- **Shared WGSL filters** are served from a GResource bundle with a "GPU
  filter" combo in Graphics settings, greyed out during software presentation.
- **GPU-composited HUD** — OSD/Lua drawing is sampled as a second layer in the
  WebGPU present pass; screenshots read the HUD back and compose it over the
  source frame, with a CPU fallback.

### Lua scripting for GTK3
- **Runtime-loaded LuaJIT / Lua 5.1** (no development headers needed), a
  script console with Run/Stop/Edit, file watching, drag-and-drop, an About
  dialog, plus frame stepping, popups, menus and host input APIs.

### Input, audio and build simplification
- **SDL-based microphone** replaces optional ALSA/OpenAL backends, removing
  two external dependencies; SDL3 then provides audio, gamepad and window
  handling across all POSIX frontends.
- **Retired GTK2 frontend removed** from Meson, autotools, CodeBlocks and CI;
  bundled AGG is built in-tree, and an AArch64 wgpu-native library lets Meson
  find the right shared object on both x86-64 and ARM64 hosts.
- **HUD layout config** now saves and restores correctly even in builds
  without libagg.
