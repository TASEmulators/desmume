# Type definitions for the DeSmuME Lua API (Lua Language Server)

`desmume.lua` is a `---@meta` definition file for the
[Lua Language Server](https://luals.github.io/). With it installed, a script
written for DeSmuME gets completion, parameter names and types for `emu.`,
`gui.`, `memory.`, `joypad.`, `stylus.`, `savestate.`, `movie.`, `input.`,
`controller.`, `sound.` and `agg.`, and a call to a function that does not exist
- or one that is passed the wrong type - is flagged as you type.

The file is not part of the emulator build and is never loaded by it. Its only
job is to describe what the Lua state created in `src/lua-engine.cpp` actually
contains.

```lua
---@meta
```

## Which Lua runs the scripts

The same `lua-engine.cpp` is built against three different engines, so pick
`runtime.version` to match the build you write scripts for:

| Front end | Engine | `runtime.version` |
| --- | --- | --- |
| Windows (MSVC) | LuaJIT 2.1 (`lua51.dll`, Lua 5.1 ABI) | `LuaJIT` |
| macOS (Cocoa) | vendored Lua 5.1.5, compiled into the app | `Lua 5.1` |
| Linux/BSD (GTK, CLI) | `luajit` if `configure` found it, otherwise `lua5.1`, otherwise `lua` | `LuaJIT` (usually) |

Lua 5.2+ syntax is not available on any of them; `goto` works only under
LuaJIT. Trace compilation is off by default even under LuaJIT
(`LUAENGINE_ENABLE_JIT`), because a JIT-compiled loop never calls the debug hook
that the engine uses to keep a runaway script stoppable.

## Installation

The editor knows two kinds of Lua library directories: a **library directory**
(`workspace.library`, which applies to every open file) and **any `.lua` file in
the workspace**. The library directory is recommended, because `desmume.lua`
does not need to be treated as a script.

**A. Alongside the scripts (recommended)** - put `desmume.lua` in a subdirectory
of your script directory and add a `.luarc.json` next to it:

```
C:\path\to\lua\
    macro.lua
    .luarc.json
    desmume-api\
        desmume.lua
```

```json
{
  "runtime.version": "LuaJIT",
  "workspace.library": ["./desmume-api"]
}
```

**B. Install once, globally** - point VS Code's `settings.json` at this
directory and every script, anywhere, gets completions:

```json
{
  "Lua.runtime.version": "Lua 5.1",
  "Lua.workspace.library": ["/path/to/desmume/documentation/lua-api"]
}
```

**C. Copy it next to the script** - put `desmume.lua` in the same directory as
your script and add `{"runtime.version": "Lua 5.1"}` there. It works, but every
`.lua` file in that directory becomes one workspace, so the scripts start seeing
each other's globals.

## Coverage

`desmume.lua` describes what this branch registers, one-to-one with the
`luaL_Reg` tables at the end of `lua-engine.cpp`:

| Table | Entries | Notes |
| --- | --- | --- |
| `emu` | 31 | includes `speedmode`, `print`, `openscript`, the 3D hooks and the menu API |
| `gui` | 26 | 256x384 canvas, ASCII-only bitmap font, includes `osdtext` and the `draw*`/`set*`/`read*` aliases |
| `memory` | 32 | all read/write widths, the `read*unsigned`/`read*long`/`read*short` aliases, VRAM access and the three hook registrations |
| `movie` | 20 | `rerecordcounting` lives here, not in `emu` |
| `joypad` | 11 | `read`/`write`/`readdown`/`readup` aliases |
| `agg` | 15 | vector shapes and text on the OSD layer |
| `stylus` | 5 | |
| `savestate` | 4 | `verify` is only registered in non-release builds |
| `input` | 4 | `input.get()` is Windows-only and returns an empty table elsewhere |
| `controller` | 2 | Windows-only, empty table elsewhere |
| `sound` | 1 | |
| globals | 7 | `AND` `OR` `XOR` `SHIFT` `BIT` `addressof` `copytable` |

Deliberately not declared:

* `bit` - LuaJIT provides its own (and the editor already knows it); a stock
  Lua 5.1 build has no `bit` table at all, so a script that uses it must guard
  with `if bit then`.
* `print` and `tostring` - the engine replaces them with its own
  implementations (they write to the script console), but the signatures are the
  standard ones, so the Lua 5.1 stubs already cover them.

## Where this API differs from the TASVideos documentation

The upstream documentation
(<https://tasvideos.org/EmulatorResources/VBA/LuaScriptingFunctions>) describes
the VBA family; DeSmuME matches it closely but not exactly. What follows is what
this build actually does.

| Item | TASVideos doc | This build |
| --- | --- | --- |
| canvas | 256x239 (VBA) | 256x384: y 0-191 upper screen, y 192-383 touch screen |
| `agg.*` coordinates | VBA's AGG layer | one screen, 0-255 by 0-191 |
| `memory.readbyterange` | string | array table, `t[1]` is the byte at `address`; a negative `length` starts at `address + length` |
| `joypad.get()` | pad number optional | controller index is optional and read from the argument list, so both `joypad.get()` and `joypad.get(0)` work |
| `emu.speedmode` | absent in VBA | `normal` / `nothrottle` / `turbo` / `maximum`, wired into frame skipping |
| `input.get()` | keyboard/mouse state | only the Windows front end fills it in; elsewhere it returns an empty table |
| `emu.addmenu`, `emu.setmenuiteminfo`, `emu.registermenustart` | menu API | Windows only; no-ops elsewhere |
| `savestate.verify` | release builds | only registered when `PUBLIC_RELEASE` is undefined, which includes the macOS debug builds |
| `gui.popup` / `input.popup` | native dialog | outside Windows the answer is guessed from the button type, not from a real dialog |

## Maintenance

After changing a `luaL_Reg` table in `lua-engine.cpp` (adding, removing or
renaming a function), update `desmume.lua` and run:

```
python3 documentation/lua-api/check_api.py
```

It extracts the function names of every `luaL_Reg` table and of every
`lua_register()` call from `lua-engine.cpp`, compares them table by table with
the declarations in `desmume.lua`, and additionally checks that every
`---@param` names a real parameter of the function below it and that every type
name is either a Lua built-in or something declared in this file. `--list` prints
the source-side API only, which is handy when writing the file from scratch.

`desmume.lua` is itself valid Lua
(`luajit -e "assert(loadfile('desmume.lua'))"` passes), but whether the
annotations *read* correctly can only be confirmed with `lua-language-server
--check`: open a workspace with this directory in `workspace.library`, write a
script that deliberately passes a string where an integer is expected, and
confirm it reports `param-type-mismatch`. A `---@meta` file does not take part in
diagnostics itself, so "no warnings in this file" is not evidence that the
annotations are right.

## Example

`examples/osd-shapes.lua` draws every `agg.*` primitive plus text in the fonts
that the OSD carries, animates them, and prints the frame counter. It is the
smoke test for the OSD drawing path: run it from the Lua Script dialog and the
upper screen should show a moving test pattern, with a second one on the touch
screen.
