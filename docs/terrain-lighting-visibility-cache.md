# Cached room-light visibility

This incremental patch requires the light-follow fix and lighting timing diagnostics.
It supports either the static-door update or the earlier animated-door code.
No artwork, shaders, movement behaviour or upstream map-update logic is changed.

## Change

Previously every lighting frame projected all permanent-light rooms to determine
visible bounds, including frames where only flicker changed. A separate scan of
all permanent lights also selected the rooms to draw inside the mask rectangle.

TerrainLightVisibility.cpp/.h now owns a small CPU cache:

- A changed projection/view matrix or focused layer rebuilds permanent-light
  visibility once. Matrix comparison is exact, so a small camera change still
  invalidates it. Resizing/zooming/rotating are covered by the resulting matrix.
- Adding, removing, moving or reseeding a permanent light invalidates the cache.
  Unrelated map changes do not invalidate an unchanged light collection.
- Flicker and brightness changes reuse the visibility result.
- The character's light is tested separately each frame, so following movement
  does not require invalidating the permanent-light visibility cache.
- The permanent lights inside the final mask rectangle are cached separately.
  They retain room-id order, including duplicate coordinates and off-screen lights
  inside that rectangle, preserving the old mask construction/overwrite behaviour.

No worker thread is introduced. Eliminating redundant work avoids worker queues,
snapshot synchronization, delayed light-follow updates and context ownership issues.
GL texture creation/upload stays on the renderer's existing thread. A view change
still incurs one visibility rebuild; the patch does not eliminate all movement,
map revision, rendering, or GPU costs.

## Apply and compare

Use the supplied guarded installer from your usual terrain-only project, then
Run CMake, Build and Run in the same Qt Creator project. No extra project,
worktree, build directory or dependency installation is required on the Mac.

Leave `MMAPPER_TERRAIN_TIMINGS=1` enabled. Compare the same zoom, window size, route
and flicker setting. Capture settled stationary flicker and walking separately.
`light-visibility` includes the cache lookup and live-light test, plus any rebuild.
The additional `light-visibility-rebuild` line counts/times only full permanent-light
visibility scans. It should be absent during stationary flicker windows after
initial setup, and appear after view/layer/light changes. Timings remain nested;
do not sum them. The later current-room patch replaces revision-triggered index
checks: `light-index` now runs only once per map load; `light-current-room`
measures subsequent single-room updates. See `terrain-lighting-current-room.md`.

## Validation

Linux Debug, Clang 18, project's strict warnings, ASAN/UBSAN:

- New cache/lighting regression tests compiled, linked and passed under CTest.
- Independent previous-selection oracle and pixel-for-pixel QImage mask comparisons
  passed for orthographic/perspective views, pans, layers, moving/off-screen/live-only
  lights, empty maps, duplicate coordinates, changed seeds/locations, deletion,
  steady light and flicker. Even a one-step floating-point matrix change invalidates.
- Changed production sources passed syntax/type checks using generated production
  compiler commands. Formatting and patch whitespace checks passed.
- A standalone benchmark with 932 lights, 200 repetitions: full visibility rebuild
  5.833 ms average; cached view plus moving-light check 0.0053 ms average, no cache
  misses in the cached loop. These are local operation timings, not whole-app
  performance or predictions of Mac results.

No complete new app link or Mac runtime/GPU benchmark was performed here. The next
check is the normal Qt Creator build and the same diagnostic comparison on the Mac.
