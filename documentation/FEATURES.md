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

### Recompiler (JIT)
- **Two maintained backends, one reference** — the x86-64 and AArch64
  recompilers are both built on current asmjit, and all 571 compilable ARM and
  Thumb opcode groups are checked against the interpreter. The 20 undefined or
  intentionally uncompiled opcodes keep using the interpreter, and the dead
  ARM32 backend is gone.
- **Verified instruction by instruction** — generated blocks are compared with
  the interpreter field by field (registers, flags and PC) on x86-64 and on
  real AArch64 hardware. The defects those comparisons exposed were fixed in
  the affected backends before the migration closed.
- **Leaner code on ARM64** — emulated registers and hot state stay in host
  registers, cutting the common ALU handlers' host instruction count roughly
  in half. On-device microbenchmarks against the interpreter show 17.75× for
  `add`, 3.55× for `ldr` and 2.05× for `str`; a whole-ROM frame-rate comparison
  is still outstanding.
- **Available in the ordinary builds** — the recompiler is wired into the
  Windows, macOS Universal/OpenEmu and POSIX builds, including native Windows
  ARM64.

### Audio engine
- **One SPU mixing path** — the rewritten mixer is now the only one; the old
  "Advanced SPU Logic" option is gone from Windows, macOS and GTK settings
  because there is no longer a second path to choose.
- **Correct after reset and load-state** — states written by this build load
  again, capture DMA resumes at the right ARM7 address, zero-length capture
  loops no longer hang, and dual-SPU audio survives a reset.
- **More interpolation choices** — GTK3 and GTK2 offer Catmull-Rom alongside
  the existing interpolation modes.

### ROM archives
- **Open ROMs straight from archives** — Windows uses the official 7-Zip 26.04
  DLL (x64 and ARM64) with a shared zlib ZIP reader as fallback; macOS and
  Linux use that same reader for ZIP and gzip, and their ROM dialogs accept
  `.zip` and `.gz` files.
- **No stale extractors** — the bundled File_Extractor, unrar, old 7-Zip copies
  and zziplib are gone.

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

### Movie metadata: one shared core
- **Header parsing without loading the movie** — DSM headers are decoded only
  up to the first frame line, so a preview can refresh on every file-selection
  change without touching frame data.
- **Every frontend shows the same numbers** — ROM serial/CRC, author, comments,
  record date, core version, length, frame count and rerecord count come from
  one place; ROM/core mismatch and the "desync expected" wording agree with
  Windows. The stored ROM checksum is read from the movie itself instead of a
  placeholder, so that warning reflects the ROM the movie was recorded with.
- **Comments go back without touching the timeline** — only comment lines are
  rewritten; the author line, unknown keys and all frame bytes survive, with an
  atomic replace on both Windows and POSIX.
- **A ready-made playback contract** — `PlayRequest`, `ParsePauseFrame()`,
  the Record helpers and the formatting functions let every frontend reuse the
  whole behaviour; only the widgets and path settings are host-specific.
  Windows, Cocoa and GTK all consume this one core.

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

### Play Movie dialog
- **The full Windows dialog with native controls** — movie file name with
  Browse/Refresh, read-only toggle, metadata preview, Comment editor,
  pause-at-frame, and read-only recording-options / firmware / BIOS groups.
- **Desync warnings before playback** — the movie's ROM serial/CRC and core
  version are compared with the loaded ROM up front; mismatched fields turn red
  and the status line says "desync expected".
- **Comments without re-recording** — opened read-write, the Comment field can
  be edited and only the header's comment lines are replaced; recorded input is
  left byte-for-byte untouched.
- **Pause at a frame (shared one-shot check)** — defaults to the last frame and
  is passed to `FCEUI_LoadMovie`. The core's `FCEUMOV_ShouldPause()` owns the
  decision (movie playing + frame reached, then clears itself); each frontend
  performs its own local pause action. Windows and Cocoa both call it from
  their frame loops, so the field is no longer inert on macOS or Linux.
- **One input-processing block per frame, as on Windows** — Cocoa now commits
  input exactly once (`ClientInputHandler::CommitInputs()`), with the Lua
  `BEFOREEMULATION` hook running inside the open block alongside movie
  playback injection. This matches Windows' `StepRunLoop_Core()` and prevents
  the hook from ever overwriting the movie's input with the physical keyboard
  (which is what made playback appear to ignore the recording). Lua's own
  `joy_set` refuses to edit a playing movie, so physical/Lua input is ignored
  during playback as intended.

### Record Movie dialog
- **All three start points** — power on (cold boot), reset/SRAM with a `.dsv`
  picker, or the current emulator state as a snapshot.
- **Author and comment up front** — both are editable and go straight into the
  movie header; the recorded timeline itself is produced by the core.
- **Start RTC and current settings** — date/time pickers seeded from the
  core's historical default, with read-only flags showing exactly which BIOS
  and firmware settings the movie will store.
- **A filename that never overwrites** — derived from the loaded ROM, with
  ` - 1`, ` - 2`, … appended until the name is free in the Movies folder.

### Path Settings
- **Tools > Path Settings** — one window for the nine folders the Cocoa frontend
  resolves through `CocoaDSFile`: ROMs, battery saves, save states, screenshots,
  video, cheats, sound samples, firmware and Lua scripts.
- **Defaults stay authoritative** — only the rows the user changed are stored in
  `NSUserDefaults`; *Use Default Folders* refills the fields from
  `FileTypeInfo.plist`, and clearing a row falls back to the bundled folder.
- **Applied to both layers** — the values feed `path.pathTo*` for the emulation
  core and `+[CocoaDSFile directoryURLByKind:]` for the open/save panels, so a
  chosen folder is where every panel starts as well.

### Metal OSD for Lua and the HUD
- **AGG-free rendering** — Lua drawing and OSD text go into a HUD layer shared by
  the live view, screenshots and recording, so all three look the same.

### WebGPU 3D renderer
- **Bundled and signed for both architectures** — WebGPU appears as a 3D engine
  choice in the Universal build, using the Metal backend.

### Recompiler and build targets
- **JIT in Universal and OpenEmu** — both targets now include the current
  recompiler, with each architecture slice building its own x86-64 or AArch64
  backend.
- **One supported Xcode project** — the unmaintained Xcode 3 project is gone,
  and stale source references in the current project have been cleaned up.

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

### Disassembler on all three frontends
- **One window, two CPUs** — the Windows tool used to open a separate window per
  CPU; it is now a single window with an ARM9/ARM7 selector, and macOS and Linux
  gained the same tool instead of being Windows-only.
- **One codebase** — decoding, address navigation, ARM/Thumb/auto mode, register
  snapshots and the breakpoint list live in the shared core, so all three
  frontends list the same instructions for the same address.
- **Breakpoints that any frontend can serve** — the CPU loop no longer posts
  messages to Windows windows, so setting a breakpoint stops emulation and shows
  the instruction everywhere.
- **Step, step over, run to return** — with the register pane writable (except
  PC) and the CPSR flags shown per bit.
- **Fixed along the way** — switching ARM/Thumb no longer drifts the address (the
  old code multiplied the line index instead of tracking an address), a
  breakpoint row is readable instead of saturated red, and closing the Windows
  window no longer leaves the tool in a half-open state.

### Build and toolchain
- **Native ARM64 build** — the solution has an ARM64 configuration beside x64,
  with a matching manifest and ARM64 builds of the bundled 7-Zip, WebGPU and
  Lua runtime libraries.
- **Current dependencies** — system DirectX headers, vendored zlib 1.4 and
  TinyXML2 11, LuaJIT headers only, and clean x64 builds (including a real Lua
  console bug).
- **Leaner tree** — the MinGW makefile, vendored Lua 5.1 sources, bundled
  DirectX/GL headers and unused extractor code are gone.

---

## Linux / POSIX (GTK4, opt-in)

### A native GTK4 frontend (WIP)
- **Try it without leaving GTK3** — Meson builds it with
  `-Dfrontend-gtk=gtk4`; GTK3 remains the default and GTK2 is still available.
  It uses native GTK4 menus and CSS, adapts to server- or client-side
  decorations, and needs GTK 4.10 or newer.
- **A complete emulator window, not a shell** — background ROM loading,
  ten instant-save slots (F1–F10 / Shift+F1–F10), screenshots, recent ROMs,
  and emulation, graphics and sound settings in one dialog. LCD layouts
  (including hybrid), screen swap, rotation and gap, plus keyboard, mouse and
  stylus input, are restored from saved preferences.
- **The familiar tools** — IO registers, RAM Watch, RAM Search, Memory Viewer,
  Disassembler, Play/Record Movie dialogs, cheats with R4 database import, and a
  Lua console with run/stop/edit, file watching and drag-and-drop. Watch and
  search rows can become cheat entries or new watches.
- **GPU presentation where available** — WGPU and OpenGL 3D backends, a Screen
  presentation selector (WebGPU backends or software/Cairo), and the shared
  GPU filter chains through an offscreen WebGPU path with a plain-blit fallback.
- **A lean frame path** — pooled frames reach the display without a full-frame
  copy; hidden windows stop presenting and inactive ones throttle to about
  10 fps while emulation and audio continue. `DESMUME_GTK4_TIMING=1` reports
  upload and snapshot timings for diagnosis.
- **Installable beside GTK3** — its desktop entry and appdata use a separate
  app-id, with a GTK4 man page and CI validation of the packaged assets.
- **A GTK4-specific disassembler view** — GTK4 dropped `GtkTreeView`, so the
  instruction list is a `GtkDrawingArea` that decodes and paints only the rows
  that fit; everything else (the core, the breakpoint list, step/step-over/run
  to return) matches the GTK3 window.
- **Current limits** — Lua popups, dynamic menus and input-state APIs still use
  generic fallbacks, there is no GPU HUD yet, and real-ROM pixel comparison and
  desktop-environment acceptance checks are still pending.

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
- **Clean switching and shutdown** — HUD resources are released before the
  WebGPU display is torn down, so switching presentation methods or quitting
  no longer crashes.

### Lua scripting
- **No development headers needed** — LuaJIT or Lua 5.1 is loaded at runtime;
  the console offers Run/Stop/Edit, file watching, drag-and-drop, an About
  dialog, frame stepping, popups, menus and host input APIs.

### Movie dialogs (Play Movie / Record Movie)
- **The same two dialogs as Windows and macOS** — File > Play movie and Record
  movie now open full dialogs instead of a bare file chooser. Header parsing,
  desync assessment, comment rewriting, unique-name generation, extension
  handling, pause-frame validation and the default RTC all come from
  `shared/movie_info_core.cpp`, the exact core the other two frontends use.
- **Play** — file path with Browse/Refresh, read-only toggle, the complete
  metadata preview (ROM serial/CRC, author, record date, core version, length,
  frames, rerecords, comments), red fields plus a desync warning when the ROM or
  core differs, a pause-at-frame field, and read-only recording/firmware/BIOS
  groups. Opening read-write lets the Comment field be edited; only header
  comment lines are rewritten, frame bytes are untouched.
- **Record** — default `<ROM> - N.dsm` name, author and comment fields, the
  three start points (power on, reset + SRAM with a `.dsv` picker, current
  state), a Start RTC calendar/time, and read-only ROM/BIOS information.
- **Stop movie is state-aware** — the menu action is disabled unless a movie is
  actually playing or recording, matching Cocoa's `isMovieActive` check. Movie
  preferences (directory, last author) persist in `[Movie]` of `config.cfg`.

### RAM Watch and RAM Search
- **The same debugger as the other ports** — live watch lists and searches over
  main RAM, ITCM and DTCM behave like the Windows and macOS windows, including
  Windows-compatible `.wch` lists, because all three are hosts over one shared
  core rather than three implementations.
- **RAM Watch** — Address / Value / Notes refreshed every frame, with New, Edit,
  Duplicate, Remove, Separator and Move Up/Down, a File menu (new, open, append,
  recent, save, save as, Auto-load, Save Window Position, close), `.wch` files
  dropped onto the list, and unsaved changes confirmed before the window closes.
- **RAM Search** — specific value, relative-to-previous, specific address and
  number-of-changes searches with the full comparison set, 1/2/4-byte widths and
  misaligned and little-endian options; Add Watch, or a double-click, sends a row
  to the watch list. A first search shows the leading 50 000 candidates and says
  how many are hidden, since `GtkTreeView` builds a row for every model entry.
- **Narrow down without starting over** — Eliminate drops a wrong candidate,
  Undo and Redo step through searches, Clear Changes restarts the change
  counters, and Poke writes the typed value into the selected address.
- **Auto-search and Add Cheat** — auto-search keeps applying the current criteria
  as the game runs and offers Retry, Ignore or Stop when the list runs dry; Add
  Cheat turns a main-RAM result into an internal cheat in the cheat list, which
  is saved with the ROM.
- **Tools > RAM Watch / RAM Search** — preferences live in the existing
  `config.cfg`, so Auto-load, window position and recent files survive restarts,
  and a new watch list is offered as `<ROM>.wch` next to the ROM.
- **Readable results** — RAM Search results and the Memory Viewer use the
  desktop's monospace font instead of a hard-coded fallback.

### Memory Viewer
- **A live hex dump of every region** — ARM9, ARM7, firmware, cartridge ROM and
  the full address space, as bytes, halfwords or words with ASCII, optional
  big-endian reads and text, raw or whole-region exports. Only visible rows are
  read, so the 4 GB view opens immediately.
- **Edited in place** — type hex to patch memory, move with the arrow keys,
  Page Up/Down, Home/End or the scrollbar and trackpad, and set read/write
  breakpoints; layout and scrolling share the same geometry on GTK3 and GTK4.

### Cheats
- **One cheat list, edited properly** — internal cheats and Action Replay or
  Codebreaker codes live in two tabs of one window, each with the columns that
  suit it. Add, Edit and Remove go through dialogs with the same validation the
  Windows dialogs use: hex addresses up to 0x0FFFFFFF, values checked against the
  selected width, a generated Action Replay preview for internal cheats, and code
  syntax that has to parse before the code can be stored.
- **Nothing is half-saved** — closing with unsaved edits asks first, and
  discarding puts the list back exactly as it was opened, so a cheat that breaks
  a game never survives by accident. Files are the same `.dct` the other ports
  read and write, one per ROM.
- **Reorder and block-edit** — Move Up/Down reorder within a tab, and ticking a
  checkbox with several rows selected turns the whole block on or off.
- **Cheat Database** — an R4 `cheat.dat`/`usrcheat.dat` can be picked once and
  then browsed per game; ticked entries are copied into the list as Action Replay
  codes.
- **Cheat Search** — width, sign and exact/compared searches against main RAM,
  narrowed repeatedly between trips back to the game, with a result list that can
  add a frozen cheat directly. It shares the `CHEATSEARCH` engine with the
  Windows dialog, so search state survives closing the window just as it does
  there.
- **Cheat > Disable cheats** — switches the whole list off without deleting it,
  and remembers the setting in `config.cfg`.

### Autofire and frame advance
- **Per-button autofire** — tick "Turbo" next to any DS button in Edit controls
  and holding that button auto-fires: pressed and released on alternate frames,
  the same pattern the Windows and macOS ports use. The switch follows the DS
  button, so keyboard and gamepad both get it, and it is remembered in the
  frontend's config.
- **Frame advance (Ctrl+N)** — stops the emulator where it is, then steps one
  frame per press. A step runs the whole frame — input latch, Lua callbacks, HUD
  and presentation — with frame skipping forced off, so what you see is the frame
  you asked for; resuming does not replay the paused time as catch-up.
- **Not to be confused with Boost** — the Boost key (default `O`) is the
  unthrottled fast-forward; autofire is the per-button one.

### ROM and Slot configuration
- **ROM Info** — File > ROM Info shows the loaded ROM title, game and maker
  codes, cartridge capacity, loaded size, CRC32, save type, and active CPU/3D
  engines. The action is unavailable until a ROM has been loaded.
- **Slot 1 and Slot 2** — Config offers device selection with descriptions.
  Slot 1 also configures the R4 FAT directory (or the loaded ROM directory) and
  debug filesystem directory; Slot 2 configures a GBA cartridge ROM and
  CompactFlash directory/image. These selections and paths persist in
  `config.cfg`.

### Path settings
- **Config > Path settings** — folders for ROMs, battery saves, SRAM
  import/export, save states and slots, screenshots, video, movies, cheats and
  Lua scripts. An empty field keeps the old default and shows it as a
  placeholder: `~/.config/desmume`, the XDG Pictures folder, or `~/Movies`.
- **The selection survives ROM loads** — battery, slot and `.dct` paths go
  through the core's `path` model, and `path.init()` no longer resets them to
  the module directory on every ROM load; the file choosers for ROMs, states,
  SRAM, recordings and Lua start in the configured folder.
- **Screenshots** — the image format is whatever GdkPixbuf can write in the
  build (PNG by default); the file name takes the Windows `%f`/`%r`/`%t` codes
  plus strftime, and an empty pattern keeps `desmume-screenshot-N`.
- **Remember the last ROM folder** — with the checkbox on, opening a ROM writes
  its folder back into the Roms row, matching the Windows frontend.

### Input, audio and build simplification
- **SDL-based microphone** replaces the optional ALSA/OpenAL backends, removing
  two external dependencies; SDL3 then covers audio, gamepad and window handling
  for every POSIX frontend.
- **Leaner tree** — AGG is built in-tree, the bundled wgpu-native library is
  found on both x86-64 and ARM64, libpcap is optional, and autotools builds link
  the shared zlib.
- **Audio options** — Catmull-Rom is available in the interpolation menu; the
  obsolete "Advanced SPU Logic" setting has been removed.
- **HUD layout** saves and restores correctly even in builds without AGG.

### GTK2 frontend: maintenance status
- **Still shipped, no longer developed** — the GTK2 frontend remains available
  through Meson, autotools, Code::Blocks and CI, alongside GTK3 and CLI.
- **Bug fixes only** — GTK2 is in maintenance status. It receives bug fixes
  only; no new features will be added to it. All new POSIX work goes to GTK3.
