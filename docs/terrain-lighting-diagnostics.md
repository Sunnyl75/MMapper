# Terrain and room-lighting timings

Diagnostic instrumentation only. Existing map update, exit handling, highlight logic,
lighting behaviour, shaders and artwork are unchanged. No additional dependency.

## Enable in the usual Qt Creator project

After applying the patch, run CMake and Build in the existing MMapper project.
In Projects > Run > Environment (expand Details if necessary), add:

```
MMAPPER_TERRAIN_TIMINGS=1
```

Run from Qt Creator. Application Output now includes `[terrain-perf]` reports.
Remove the variable, or set it to 0, and restart to disable diagnostics. Disabled
instrumentation does not read the clock for each timing scope, collect samples or log.
No diagnostic timer, redraw request, GL query or forced GPU synchronization is added.

Keep the existing performance statistics display off for this experiment: that
existing display calls glFinish, which changes performance. Reports flag it as
`blocking-perf-display=1` when enabled.

## Compare

Keep the map, zoom, window size and route the same. Allow initial loading to finish.
Walk a short route for about 15 seconds in each mode:

1. Room illumination off.
2. Room illumination on, flicker 0%.
3. Room illumination on, flicker at the amount normally used.

Repeat in reverse order if the difference is small. Copy the `[terrain-perf]` lines
from Application Output for analysis. No new project, dependency installation or
separate build directory is needed. These diagnostics do not commit or push.

## Read the reports

Each stage has a sample count, average, maximum and total wall time in milliseconds.
Stages are nested: **do not add them together**. Reports occur after a rendered
frame when at least three seconds have elapsed, or on the next rendered frame
after a lighting setting changes. They do not cause extra frames. With steady or
disabled lighting, reports can therefore be less frequent. Settings changes flush
the previous samples before starting a new group.

| Label | Measurement |
| --- | --- |
| frame-cpu | CPU wall time for an accepted canvas frame, including the stages below |
| pending-gl | Pending texture/render configuration work |
| batches | Batch update and completion of pending meshes |
| draw-submit | Canvas drawing and lighting preparation; includes any driver waits in those calls |
| light-prepare | Entire room-light preparation, including any index/visibility/mask/upload work |
| light-index | One initial lighting-index scan per map load (first active detailed view) |
| light-current-room | Update of the entered room after MMapper applies its parsed changes; no full-map scan |
| light-visibility | Visibility cache lookup/refresh and current-room light selection |
| light-visibility-rebuild | Full permanent-light visibility scan, only when the view/layer or light index changes |
| mask-build | CPU mask image construction; includes first-use atlas loading |
| texture-upload-submit | Texture allocation/update and lookup bookkeeping; CPU submission time, not GPU completion |
| gmcp-observe | Our lighting observation callback |
| latest-gmcp-to-frame-submit | Latest EventMoved received by our observer to end of the next accepted frame's CPU work |
| recenter-to-frame-submit | Latest canvas onMovement call to end of the next accepted frame's CPU work |

The movement metrics are **not keypress-to-screen latency**. Notifications can be
coalesced before a frame; the report counts superseded movement notifications.
The next frame can precede completion of room matching; neither metric proves the
new room/marker was already presented. Recenter also occurs when loading a map or
centering manually. Network delay, work before the observer callback, GPU execution,
compositing and display presentation are not measured by these intervals.

`active-light-frames` counts frames with an active lighting texture. `cached-lights`
is the total permanent-light index size, not a count of visible rooms. `mask` gives
the last uploaded dimensions (retained even after lights are disabled). `uploaded`
is the RGBA payload submitted in that report window; it resets between reports.
Initial scans, first-time texture creation and setting transitions can be slower
than settled movement. Separate those from later samples.

Diagnostics live in TerrainLightingDiagnostics.cpp/.h and small hooks in our
lighting and canvas integration. They do not alter the master's Map/World/PathMachine
or asynchronous highlight calculations. GPU-heavy rendering can still be expensive
while CPU submission appears inexpensive; low CPU timings alone do not rule that out.

## Movement scheduling extension

The subsequent movement-diagnostics patch adds `[terrain-move]` traces separating
observer-to-request, request-to-paint-attempt, request-to-accepted-frame and frame
CPU work. It also records tracked request causes, refused paint attempts and
configured maximum FPS. See `terrain-movement-diagnostics.md` for the three-mode
comparison and interpretation limits. The original next-frame aggregate can
precede room matching; use the new matched traces to investigate movement delay.
