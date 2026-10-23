# DeSmuME — Volt Hunt experimental Windows instrumentation

This branch contains experimental Windows DeSmuME hooks. It is **not** a verified NSMB physics extractor. Compile success alone does not verify runtime, save-state alignment, or input determinism.

## Enable tracing

Before starting DeSmuME, set two Windows environment variables:

- `VH_TRACE_ADDRS=02000000,02000004` — 1–4 aligned ARM9 main RAM addresses in hexadecimal, from `02000000` through `023FFFFC`.
- `VH_INPUT_SCRIPT=C:\\path\\to\\experiment.txt` — optional input schedule with one `start_frame,end_frame,hex_button_mask` per line (see `experiment-example.txt`).

Example Windows Command Prompt:

```bat
set VH_TRACE_ADDRS=02000000,02000004
set VH_INPUT_SCRIPT=C:\\volt-hunt\\experiment.txt
DeSmuME.exe
```

Tracing writes `volt_hunt_trace.csv` to the process working directory. Columns are `frame,effective_input_hex,0x02000000,...`. ARM9 values are unsigned 32-bit *raw* readings, not automatically decoded as position or velocity.

Input playback only starts counting when the emulator executes frames. It also replaces controller inputs with a neutral state on frames without an active interval **if a valid script was loaded**. These frame numbers are **not tied to a savestate**. Load your ROM, load the playable savestate, and explicitly validate alignment before analyzing data. Loading a different ROM resets the trace and script state.

## Limitations

- Data capture and controller playback are source modifications; real gameplay behavior is not yet verified.
- Scripted buttons are inserted after Lua/movie playback, so interaction with those features may be non-deterministic. Avoid enabling them in measurements.
- Sampling occurs after `NDS_exec`, and the CSV is flushed each frame (performance cost).
- This implementation is primarily a research scaffold. ARM instruction/write tracing, automatic memory discovery, and physics constant extraction are not implemented.
- No ROMs are included. Bring your own lawfully obtained game copy.
