# Video filter notes

This file records the state of the CPU video filter (`filter/videofilter.cpp`)
after making its worker threads lazy and configurable, and lists the bugs that
were found while testing that change.

## What changed

* `VideoFilter` no longer creates worker threads for
  `VideoFilterTypeID_None`. Threads are created when a CPU filter is selected
  and destroyed when switching back to `None`.
* The requested worker count is remembered in `_requestedThreadCount`, so
  changing filters does not reset the user's setting.
* New public API: `VideoFilter::SetThreadCount(size_t)` and
  `VideoFilter::GetThreadCount()`.
* Per-thread source ranges are now distributed in whole filter blocks
  (`scaleDivide` for the 1.5x filters), and each thread's destination range is
  derived from its source range. Previously the source and destination ranges
  were divided independently, so a thread could produce a different number of
  destination lines than the range it was given and overwrite its neighbours.
* `RunFilter()` skips threads that were assigned no rows (see bug 3).
* GTK3 (and GTK2) have a new "Video filter threads" setting:
  `View/FilterThreads` in the config file. `0` means automatic, which resolves
  to `NDS_GetCPUCoreCount()` at runtime; values are clamped to 1..64 when
  applied.
* Windows uses `VideoFilter` for display and composed recording. The 3D settings
  dialog's "Video filter threads" controls both paths: `0` runs synchronously
  (the default), and `1..64` selects worker threads. The recording snapshots
  this setting when it starts. Threaded filters may differ at slice boundaries
  as described below.
* Windows stores shared filter IDs in `Video/Filter ID` and
  `Recording/Picture Filter ID`. The older keys used a different ID order and
  are not read; missing or invalid values select `None`.

## Bugs found

### Fixed

1. **Idle worker threads for `VideoFilterTypeID_None`.**
   Every `VideoFilter` instance spawned `threadCount` worker threads even when
   no CPU filter was active, so e.g. the GTK frontends kept
   `num_cores` threads alive while the default "None" filter was selected.
   Threads are now created/destroyed with the filter.

2. **Source/destination row mismatch between threads.**
   The destination height was divided by the thread count independently of the
   source height. With heights that are not a multiple of the thread count, and
   with the 1.5x filters (which consume 2 source lines to produce 3 destination
   lines), a thread's destination range did not match the lines its filter call
   produced, so threads wrote into each other's output. Destination ranges are
   now computed from the source boundaries, and source lines are handed out in
   `scaleDivide`-sized blocks.

3. **Filters called with zero-height surfaces.**
   When there were more threads than source lines, some threads received an
   empty range. `RunFilter()` still invoked the filter for them, and several
   filters (LQ2xS, HQ2x, HQ3x, HQ4x, ...) write a complete line pair/block
   unconditionally, so they wrote past the end of the destination buffer
   ("double free or corruption", "malloc(): invalid size"). `RunFilter()` now
   skips threads whose source or destination height is zero.

4. **Infinite loop in HQ2x/HQ3x/HQ4x for a single source line.**
   `hq2x32()`, `hq2xS32()`, `hq3x32()`, `hq3x32S()`, `hq4x32()` and
   `hq4x32S()` did `count = height - 2; while (count)`. For `height == 1`,
   `count` is `-1` and the loop never terminates, walking the destination
   pointer off the buffer. The loops now use `while (count > 0)`.

5. **HQ2x/HQ2xS/HQ3x/HQ3xS/HQ4x/HQ4xS wrote a trailing block for one line.**
   These functions always emit a final output block after the main loop. For
   `height == 1` the first call already produced the whole destination, so the
   trailing block wrote one extra block past the buffer. Added an early return
   for `height == 1`.

8. **Bilinear sampled the next source row one pixel to the right.**
   `Bilinear32()`/`BilinearPlus32()` (`filter/bilinear.cpp`) called
   `fill_rgb_row_32(from + width + 1, ...)` for the row below. `from` is a
   `u32*` at the start of the current row, so the next row starts at
   `from + width` (the source surface is tightly packed), not `from + width + 1`.
   Every lower-left/lower-right destination pixel was therefore blended with the
   pixel one column to the right of the intended one, and the last row read one
   pixel past the buffer. The 16-bit `Bilinear()` and VBA's `Bilinear32()` both
   use the row start, which is what confirms the off-by-one. Now stepped by
   `srcPitch` so it also stays correct if the surface ever gains padding.

9. **Bilinear's scratch rows were shared between worker threads.**
   `row_cur`/`row_next` were file-scope `static` buffers, but `VideoFilter`
   calls the filter entry point once per worker thread on one horizontal slice
   each. With four threads the threads overwrote each other's unpacked rows and
   ~60% of the destination pixels were wrong (119100 of 196608 for 256x192).
   They are `thread_local` now; what remains is the usual one-row seam at each
   thread boundary (bug 7).

10. **Scanline kept its factors in shared statics.**
    `RenderScanline()` recomputed `fac_a..fac_d` into file-scope statics on every
    call. All threads store the same values, so the output did not change, but
    the writes raced. The factors are local variables now.

### Open

6. **1.5x filters overrun the destination for odd source heights.**
   `RenderNearest_1Point5x()` and `RenderNearestPlus_1Point5x()`
   (`filter/epx.cpp`) iterate `yi += 2` and always write a full 3-line output
   block, but the destination height is `srcHeight * 3 / 2` (integer
   division). For an odd source height (e.g. 1 or 7) the last iteration reads
   and writes one line past the destination buffer. Valgrind reports:

   ```
   Invalid write of size 4
      at RenderNearest_1Point5x(SSurface, SSurface) (epx.cpp:248)
      by RunVideoFilterTask(void*) (videofilter.cpp)
   ```

   The normal frontends use even heights (192/384 and multiples), so this is
   not hit in regular use, but it makes the stress test abort on the odd-height
   cases. Not fixed yet.

7. **Multi-threaded output differs slightly at thread boundaries.**
   Edge-aware filters (HQ2x, HQ2xS, HQ3xS, HQ4x, LQ2xS, ...) duplicate the
   first/last row of the surface they are given instead of reading across the
   boundary. When the image is split between threads, the rows next to each
   split therefore differ from the single-threaded result. This is pre-existing
   behaviour (the old code split the image the same way) and is limited to a
   few rows per boundary; it is not a correctness problem for the output as a
   whole, but it means multi-threaded output is not bit-identical to
   single-threaded output. The stress test therefore ignores differences within
   `2 * scaleMultiply` rows of a thread boundary.

## Comparison with VBA-RR's filters

`github.com/CarterLi/vba-rr-modernized/src/filters` was compared filter by filter.
Method: the DeSmuME entry points (`RenderXxx(SSurface, SSurface)`) and the VBA-RR
entry points (`xxx(u8*, srcPitch, delta, u8*, dstPitch, w, h)`, byte pitches) were
compiled into one binary, run over the same ARGB images at 256x192 (plus 1x1,
1xW, Hx1, 2x2 and odd sizes), and compared pixel by pixel, with ASAN for the
out-of-bounds checks. Timings are best of 5 x 40 iterations at 256x192 on Apple
Silicon, `-O2`, i.e. the scalar paths that macOS actually builds.

| filter | output vs DeSmuME | VBA time / DeSmuME time |
| --- | --- | --- |
| HQ2x | identical | 1.00 |
| HQ2xS | differs, first/last column of every row | 0.96 |
| LQ2x | identical | 1.00 |
| HQ3x | ~3.1% of pixels differ (other edge metric) | 1.56 |
| HQ4x | ~3.7% of pixels differ | 1.39 |
| 2xSaI | 1 pixel per row (left edge) | 1.29 |
| Super 2xSaI | same | 1.30 |
| Super Eagle | same | 1.56 |
| Bilinear | wrong colours (see below) | 0.90 |
| Scanline | different effect (blanks the line) | 0.15 |
| EPX vs AdMAME2x | 1 pixel | 1.66 |
| Nearest 2x vs Simple2x | identical | 1.71 |

**Verdict: nothing should be replaced.** Every VBA-RR filter that computes the
same result is equal or slower on this platform, and the ones that are faster
either compute a different picture (Scanline) or are broken (Bilinear). Its
`xbrz.cpp` is not a candidate either: `filter/xbrz19/` already *is* the VBA-M
rework of xBRZ 1.9, including its SSE2/NEON paths.

The bugs that VBA-RR still carries, i.e. exactly the classes fixed here:

* `hq2x32()`, `hq2xS32()`, `lq2x32()`: `count = height - 2; while (count)` and no
  `height == 1` early return - bugs 4 and 5 above, still present. `height == 1`
  runs away and dies with SIGBUS in the harness.
* `hq2xS_32_def()`: reads `src0[-1]`/`src0[1]` without the `i > 0` /
  `i < count - 1` guards, so it reads outside the row at both ends (ASAN:
  heap-buffer-overflow at `hq2x.cpp:433`) and the first/last column picks up the
  neighbouring row. This is why its output differs from `RenderHQ2XS()`.
* `internal_scale2x_32_def()` (AdMAME2x = EPX): `unsigned count; count -= 2;
  while (count)` - for `width == 1` the count underflows and the loop runs away.
* `Scanlines32()`/`ScanlinesTV32()`: `do { ... } while (--height)` hangs for
  `height == 0`, and the scalar 32-bit path writes two pixels past every row when
  `width` is odd (255x191, 7x5, 1x1 all trip a guard word).
* 16-bit `hq3x32()`: `if ((j > 0) || (j < Yres - 1)) line = srcPitch;` should be
  `&&`, so the first and last source row reach one row outside the surface. The
  32-bit path has the correct `&&`; DeSmuME never uses the 16-bit one.
* `Bilinear32()` scalar path: `fill_rgb_row_32()` is a copy of the 16-bit loader
  (`& 0x1f`, `<< 3`) and `RGB()` shifts by 3/4, so 32bpp input is unpacked as if
  it were 15/16bpp - the colours come out wrong. Its SSE2 path is fine.

What *is* worth importing is the vectorisation, not the filters:

* `Bilinear32_SSE2()` averages packed ARGB with `_mm_avg_epu8` and needs no
  scratch rows at all. Built through `sse2neon` on arm64 it is bit-identical to
  the fixed `RenderBilinear()` for 5-bit-expanded colours (which is all the DS
  produces) and 10.5x faster: 19.9 us vs 208.4 us.
* `AdMame2x32` through the same path: identical output to `RenderEPX()`,
  26.0 us vs 64.1 us (2.5x).

Note that VBA-RR's own macOS build gets none of this: `simd_common.h` only
enables `USE_SSE2` for `_M_ARM64` (MSVC) or real SSE2, so clang/arm64 compiles the
scalar fallbacks that were measured above.

The two kernels are now implemented here using native SSE2 or NEON through
`filter_simd.h`, with the existing scalar implementations retained as fallback.
The SIMD paths match the scalar output across the tested dimensions, including
small widths and vector tails. On Apple Silicon at `-O2`, the standalone
256x192 probe averaged 41.0 us vs 215.9 us for Bilinear32 (5.3x), and 24.8 us
vs 66.0 us for EPX (2.7x), over five runs. These are isolated single-threaded
kernel timings; whole-frame gains depend on the filter pipeline and threading.

## Tests

Standalone probes live in `filter/tests/`:

* `videofilter_stress_test.cpp` - creates a `VideoFilter` for every filter
  type, several image sizes and thread counts (including 0 and 1), checks lazy
  creation/teardown, verifies destination dimensions, compares output against a
  single-threaded reference (ignoring boundary rows, see bug 7), changes the
  thread count while a filter is active, switches back to `None` and back, and
  checks that the requested thread count is restored.
* `videofilter_boundary_probe.cpp` - prints which destination rows differ
  between a single-threaded and a multi-threaded run, to show that differences
  are confined to thread boundaries.
* `filter_simd_test.cpp` - checks Bilinear32 and EPX output against scalar
  references on vector boundaries, odd sizes, and 256x192, and checks that
  destination row padding remains untouched.
* `windows_video_filter_test.cpp` - checks display and composed recording
  dimensions and pixels for every filter while switching between synchronous
  and four-thread execution, invalid display filter IDs, and 2x GPU prescaling.

Build and run them against the static library produced by the meson/ninja
build (from the repository root, adjust the `-I` and library paths as needed):

```
g++ -std=c++14 -O1 -g -Idesmume/src -Idesmume/src/libretro-common/include \
    desmume/src/filter/tests/videofilter_stress_test.cpp \
    -Wl,--start-group <build-dir>/libdesmume.a -Wl,--end-group \
    -lpthread -lz -o /tmp/videofilter_stress_test
/tmp/videofilter_stress_test
```

The boundary probe takes an optional filter type ID and thread count:

```
/tmp/videofilter_boundary_probe 3 4    # HQ2x with 4 threads
```

The SIMD consistency test can be built directly with the filter sources:

```
clang++ -std=c++14 -O2 -Idesmume/src -Idesmume/src/libretro-common/include \
    desmume/src/filter/tests/filter_simd_test.cpp \
    desmume/src/filter/bilinear.cpp desmume/src/filter/epx.cpp \
    -o /tmp/filter_simd_test
/tmp/filter_simd_test
```

Known result at the time of writing: the stress test passes for the even
heights, but aborts when it reaches the odd-height cases of the 1.5x filters
(bug 6).
