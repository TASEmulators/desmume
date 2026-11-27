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

## Tests

Two standalone probes live in `filter/tests/`:

* `videofilter_stress_test.cpp` - creates a `VideoFilter` for every filter
  type, several image sizes and thread counts (including 0 and 1), checks lazy
  creation/teardown, verifies destination dimensions, compares output against a
  single-threaded reference (ignoring boundary rows, see bug 7), changes the
  thread count while a filter is active, switches back to `None` and back, and
  checks that the requested thread count is restored.
* `videofilter_boundary_probe.cpp` - prints which destination rows differ
  between a single-threaded and a multi-threaded run, to show that differences
  are confined to thread boundaries.

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

Known result at the time of writing: the stress test passes for the even
heights, but aborts when it reaches the odd-height cases of the 1.5x filters
(bug 6).
