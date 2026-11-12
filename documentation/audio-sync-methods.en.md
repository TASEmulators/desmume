# DeSmuME Audio Synchronization Methods

[中文版](audio-sync-methods.zh-CN.md)

This document describes the audio synchronization methods supported by the DeSmuME
core. It covers why synchronization is needed, how the three methods `N`, `Z`, and
`P` differ, and which one to choose.

## 1. Where the methods live

Audio synchronization is implemented by **metaspu**, a small subsystem ported from
the PCSX2 SPU2-X plugin. The relevant configuration is two-level:

| Level | Enum | Values | Meaning |
| --- | --- | --- | --- |
| Sync mode | `ESynchMode` | `ESynchMode_DualSynchAsynch`, `ESynchMode_Synchronous` | Whether the output goes through the metaspu sample queue at all |
| Sync method | `ESynchMethod` | `ESynchMethod_N`, `ESynchMethod_Z`, `ESynchMethod_P` | The algorithm used when the mode is `Synchronous` |

`Dual Asynchronous` is therefore a separate **mode**, not one of the three
**methods**. When `Dual Asynchronous` is selected, the synchronizer is bypassed:
audio is mixed on demand for the output buffer and no rate correction is applied.
The three methods below are the choices available under `Synchronous`.

Defaults (`src/NDSSystem.cpp`):

```cpp
SPU_sync_mode = ESynchMode_Synchronous;
SPU_sync_method = ESynchMethod_N;
```

The method is user-selectable from each front end:

- GTK: *Audio Synchronization* menu
- Windows: *Config > Sound Settings*, "Synchronizer Method" group
- Cocoa: *Preferences > Sound*, "SPU Sound Synchronization Method"

`P` is only offered when the build includes SoundTouch.

## 2. Why synchronization is needed

The DS SPU produces PCM based on emulation timing, while the host audio device
consumes PCM at a fixed rate. The two clocks are never exactly equal: emulation
speed fluctuates around 100%, and the audio device runs on its own clock.

Without correction, the buffer between the producer and the consumer slowly
drifts:

- **Underrun** — the consumer asks for samples that have not been produced yet.
  The result is a gap, a pop, or a stutter.
- **Overrun** — the producer has nowhere to put new samples. The result is
  dropped audio and a burst of static.

The synchronizer sits between the SPU and the audio device and compensates for
this drift. There are two fundamentally different strategies:

1. **Resample / skip samples** — keep the producer rate untouched and let the
   consumer read the queue slightly faster or slower.
2. **Time-stretch** — change the playback speed of the audio itself while
   trying to preserve pitch.

Methods `N` and `Z` use the first strategy; method `P` uses the second.

Data flow with a synchronous method:

```
SPU mixer
   │  PCM frames
   ▼
ISynchronizingAudioBuffer  (N / Z / P)
   │  rate-corrected PCM
   ▼
Host audio device
```

## 3. Comparison at a glance

| | **N** | **Z** | **P** |
| --- | --- | --- | --- |
| UI label | N: Crossfade resampler (low CPU) | Z: Micro-resampler (low latency) | P: Time-stretch (keeps pitch) |
| Class | `NitsujaSynchronizer` | `ZeromusSynchronizer` | `PCSX2Synchronizer` + `SndBuffer` |
| Strategy | Crossfade / ping-pong over a sample queue | Continuous micro-resampling | SoundTouch time-stretching |
| External dependency | None | None | SoundTouch (optional) |
| Nominal latency | Low | Very low | Higher |
| CPU cost | Low | Very low | Highest |
| Pitch during correction | Not preserved during resampling | Slight drift | Preserved |
| Typical artifacts | Crossfade smearing; short reverse playback on severe slowdown | Faint aliasing/clicks on tonal material | Time-stretch smearing / warbling |
| Available in | All builds | All builds | Builds with `HAVE_LIBSOUNDTOUCH` or `DESMUME_COCOA` / MSVC |

## 4. Method N — Nitsuja

**Implementation:** `NitsujaSynchronizer` in `src/metaspu/metaspu.cpp`

### How it works

`N` maintains a plain `std::vector` queue of stereo frames. On each output
request it looks at the queue length and the number of requested frames:

- If there is little or no data (`queued <= 0x200`, i.e. 512 frames), it
  outputs nothing and waits. This is the underrun protection.
- If the queue and the request are of similar size, it copies frames straight
  through. This is the normal-speed path.
- If the queue is much longer than requested (`queued > 900` or
  `audiosize > queued * 2`), it resamples:
  - **Fast-forward** (`audiosize <= queued`): it crossfades between the
    beginning and the end of the queue, producing fewer output frames without
    an abrupt cut.
  - **Slow motion** (`audiosize > queued`): it selects samples using a
    ping-pong index, so the playback position sweeps forward and then briefly
    backward instead of jumping. It also searches for endpoints with a small
    slope to reduce clicks.

There is no DSP, no external library, and no persistent sample-rate conversion.
The correction is applied in bursts when the queue drifts out of the normal
window.

### Characteristics

- **Pros:** zero dependencies, very cheap, works on every build, and is the
  historical default.
- **Cons:** during fast-forward it crossfades, which can smear transients; during
  slow motion it deliberately plays short stretches backward (audible only on
  large corrections). It is a "best effort" resampler rather than a continuous
  clock corrector.
- **Best for:** the general case when the emulator runs close to 100% speed and
  a simple, dependency-free method is preferred.

### Source

```
src/metaspu/metaspu.cpp   NitsujaSynchronizer (class starts around line 191)
src/metaspu/metaspu.h     ESynchMethod_N
```

## 5. Method Z — Zeromus

**Implementation:** `ZeromusSynchronizer` in `src/metaspu/metaspu.cpp`

### How it works

`Z` keeps the audio in an `Adjustobuf` queue and applies a very small,
continuous playback-rate correction. Unlike `N`, it does not wait for a large
error before acting; it nudges the rate every time a frame is consumed.

Key parameters:

```cpp
// release builds
adjustobuf(200, 1000)      // minLatency, maxLatency in frames
// debug builds
adjustobuf(22000, 44000)   // much larger window for debugging

kAverageSize = 80000;      // moving-average window, in output frames
targetLatency = (minLatency + maxLatency) / 2;
```

On every dequeue it records the current queue size and computes a moving
average over the last `kAverageSize` frames (80,000 frames ≈ 1.8 s at 44.1 kHz).
The target playback rate is then:

```cpp
if (averageSize < targetLatency)
    targetRate = 1.0f - (targetLatency - averageSize) / kAverageSize;
else if (averageSize > targetLatency)
    targetRate = 1.0f + (averageSize - targetLatency) / kAverageSize;
else
    targetRate = 1.0f;
```

Because the divisor is `kAverageSize`, even a large latency error produces a
rate change of only a fraction of a percent. For example, with the release
window the rate stays within roughly `0.99–1.01`. The correction is therefore
spread over seconds and is usually inaudible.

Sample consumption is done with a fractional cursor:

```cpp
cursor += rate;
while (cursor > 1.0f) {
    cursor -= 1.0f;
    curr = buffer.pop();   // advance one source frame
}
output curr;               // hold the current frame otherwise
```

This is nearest-neighbor / sample-and-hold resampling: frames are occasionally
repeated or skipped, but only at a very low rate.

`mixqueue_go` adds a small startup gate: playback does not begin until the queue
holds more than 200 frames, and it stops when the queue is empty.

### Characteristics

- **Pros:** extremely low latency (nominal target ∼600 frames ≈ 14 ms at
  44.1 kHz; window 200–1000 frames), very low CPU cost, no dependencies, and a
  smooth correction that rarely produces audible clicks.
- **Cons:** there is no pitch-preserving time stretch, so the tiny rate changes
  also shift pitch slightly (on the order of a few cents). On sustained tonal
  material, sample repetition/skipping can introduce very faint artifacts.
- **Best for:** low-latency setups, and users who want smooth drift correction
  without adding a library dependency.

### Source

```
src/metaspu/metaspu.cpp   ZeromusSynchronizer (class starts around line 49)
src/metaspu/metaspu.h     ESynchMethod_Z
```

## 6. Method P — PCSX2 / SPU2-X

**Implementation:** `PCSX2Synchronizer` in `src/metaspu/metaspu.cpp`, backed by
`SndBuffer` (`src/metaspu/SndOut.cpp`) and the SoundTouch library
(`src/metaspu/Timestretcher.cpp`).

### How it works

`P` is the SPU2-X approach. PCM frames are pushed into `SndBuffer`, a ring
buffer whose target is roughly 50% full. A separate layer converts frames to
float, feeds them to SoundTouch, and reads back time-stretched frames.

SoundTouch changes the **tempo** of the stream while keeping the pitch. The
tempo is driven by the ring-buffer fill level:

```cpp
// GetStatusPct(): 0.0 = 50% full, +1.0 = full, -1.0 = empty
pSoundTouch->setTempo(newTempo);
```

The tempo is clamped to `0.10–10.0`. Small deviations within roughly
`0.97–1.045` are treated as "normal speed" and the tempo slides back toward
`1.0`; only larger deviations trigger real stretching.

When the buffer runs dry or overflows, the method applies an emergency tempo
adjustment (`timeStretchUnderrun()` / `timeStretchOverrun()`) and, on overflow,
discards some buffered frames to make room.

With the default `SndOutLatencyMS = 160`, the ring buffer is sized at
`160 ms × 2.0 = 320 ms`, targeting about 160 ms of buffered audio. This is
significantly more latency than `N` or `Z`, but it gives the time-stretcher room
to work smoothly.

### Characteristics

- **Pros:** preserves pitch, handles sustained drift and speed deviations
  gracefully, and is the most "musical" of the three when the ratio changes.
- **Cons:** highest CPU cost, highest latency, and it requires SoundTouch. Large
  or rapidly changing corrections can produce time-stretch artifacts (smearing
  or warbling). It is unavailable in builds without SoundTouch.
- **Best for:** users who care about pitch preservation and can afford the extra
  latency; also useful when emulation speed deviates noticeably from 100%.

### Availability

`P` is compiled only when one of the following is true:

- `HAVE_LIBSOUNDTOUCH` is defined (Meson always defines it; Autotools defines it when it finds a system SoundTouch)
- `DESMUME_COCOA` is defined (macOS builds)
- `_MSC_VER` is defined (Windows builds, which compile the bundled copy)

The bundled SoundTouch in `src/metaspu/SoundTouch/` is version **2.4.1** and is
licensed under **LGPL 2.1**. Meson prefers the system library and falls back to
compiling the bundled sources when it is unavailable; Autotools requires a
system SoundTouch. Windows and macOS compile the bundled sources directly. The
bundled library builds on x86, x64, and ARM64; on ARM64 it uses the portable C++
path, since the MMX/SSE optimizations are x86-only.

### Source

```
src/metaspu/metaspu.cpp          PCSX2Synchronizer
src/metaspu/SndOut.cpp           SndBuffer: ring buffer, underrun/overrun handling
src/metaspu/Timestretcher.cpp     SoundTouch integration, tempo control
src/metaspu/SoundTouch/          bundled SoundTouch 2.4.1 (LGPL 2.1)
```

## 7. Choosing a method

| Situation | Suggested method |
| --- | --- |
| Default / just want it to work | **N** |
| Lowest possible latency, no extra dependency | **Z** |
| Pitch must stay constant, latency is acceptable | **P** |
| Emulation speed is close to 100% and CPU is limited | **N** or **Z** |
| Frequent speed changes (fast-forward, slow hardware) | **P** |
| Build without SoundTouch | **N** or **Z** |
| Tonal music sounds slightly detuned with N/Z | **P** |

There is no universally "best" method. `N` and `Z` trade pitch accuracy for
latency and CPU; `P` trades latency and CPU for pitch accuracy. The audible
difference is usually small when the emulator is running at full speed.

## 8. Relationship to other audio settings

- **Audio synchronization** selects the mode/method described here.
- **Audio interpolation** (`None`, `Linear`, `Cosine`, `Cubic`, etc.) is a
  separate DS SPU resampling quality setting and is applied before
  synchronization.
- **Sound buffer size / latency** (front-end specific) changes the amount of
  buffering and therefore how much correction the synchronizer must perform.
  It does not select the algorithm.

## 9. Limitations

- The methods correct **drift**; they do not fix a host audio device that is
  fundamentally broken or a front end that cannot keep up.
- `N` and `Z` do not preserve pitch while correcting.
- `P` cannot eliminate latency; it deliberately buffers more audio.
- Exact latency and artifact behavior depend on the front end, the buffer size,
  and the host audio backend.
