# Movement request and frame timing diagnostics

Incremental instrumentation after the current-room lighting index patch. It does
not change frame scheduling, the frame limiter, flicker cadence, lighting, map
updates or movement handling. `MMAPPER_TERRAIN_TIMINGS=1` enables it together with
the existing diagnostics; unset/0 disables it after restarting.

## Capture

In the existing Qt Creator project, Run CMake, Build and Run. Keep the existing
performance-statistics display off: that display forces GPU synchronization.

For the same map, route, zoom and window size, compare:

1. Room illumination off.
2. Illumination on, flicker 0%.
3. Illumination on, normal flicker.

Wait a few seconds after changing a setting, walk several rooms, then wait another
five seconds to include the final report. Do not manually recenter during a trace.
Send `[terrain-move]`, `[terrain-perf] lights=...`, redraw-request summaries and any
surrounding map-update timings. The new settings header also records `max-fps`.

## Per-movement trace

A `[terrain-move]` line appears at completion of the first accepted frame after a
movement/recenter redraw request associated with a pending EventMoved notification.
An early paint from raw GMCP observation, before that processed-room request, does
not consume the trace. Events coalesced before a frame retain the latest EventMoved;
sequence numbers may skip. Repeated processed-room requests retain both the FIRST
and latest request timestamps, so a later request cannot hide an earlier wait.

| Field | Meaning |
| --- | --- |
| seq | Local movement-observer sequence number; not a server ID |
| gmcp-to-request | EventMoved received by our observer to first processed movement redraw request |
| request-to-attempt | First movement request to the first following host paint attempt |
| request-to-frame-start | First movement request to start of an accepted frame |
| latest-request-to-start | Latest movement request to accepted frame start |
| frame-cpu | Accepted frame start to end of CPU drawing/submission |
| gmcp-to-submit | Whole traced interval from our EventMoved callback to submission |
| movement-requests | Movement/recenter requests associated with that trace |
| rejected-paints | Host paint attempts refused by FrameManager before acceptance |
| flicker-age-at-request | Milliseconds since the most recent observed flicker timer request; -1 means none observed |
| flicker-ticks-after-request | Flicker timer requests between first movement request and frame acceptance |
| frame-requests | Flags for our tracked request types since the preceding accepted frame |
| max-fps | Configured maximum FPS, read without modifying it |

Request flags can include movement, flicker, observation and settings together.
They record requests, not the sole trigger of the frame. Other pre-existing requests,
FrameManager heartbeats and OS expose/compositor events are not labelled at source.
Flicker being present does not by itself establish that it held up movement.

A large request-to-attempt suggests delay before the host paints. An early attempt
followed by a larger request-to-frame-start and refused attempts points toward
frame acceptance/limiting. A small request-to-frame-start followed by a large
frame-cpu points toward rendering/submission work. Compare all three lighting modes
before attributing a delay to lighting. The request-to-start interval includes the
FrameManager acceptance decision and any settings-report work immediately before
frame timing; it is not a pure measurement of idle waiting.

These are CPU wall intervals, not keypress-to-screen, network, GPU execution or
presentation timings. OnMovement can also be called for loading/manual recentering;
without a pending EventMoved it produces no new movement trace. The existing legacy
`latest-gmcp-to-frame-submit` metric still refers to the next accepted frame, which
may precede room matching. Prefer the new matched trace for this investigation.

## Summaries and overhead

Existing reports gain `gmcp-to-movement-request`,
`movement-request-to-first-paint-attempt`, `movement-request-to-frame-start`, and
`matched-gmcp-to-frame-submit`. Counts can differ across coalescing/setting changes;
use one trace's components when decomposing a specific movement delay.
`redraw-requests` counts our tracked causes and all rejected paint attempts in the
report window. Original coalesced-move accounting now also notices a trace that
survived an early pre-matching frame.

No timer, extra redraw, GL wait/query, file output or map traversal is added. With
logging enabled, a few steady-clock reads and counters are collected, with one
extra log line per matched movement trace. Clock reads and collection are bypassed
when diagnostics are disabled. Diagnostic logging itself has some overhead.

## Validation

Linux Debug/Clang 18 with the project's strict warning flags and ccache:

- Changed production sources TerrainLightingDiagnostics.cpp, mapcanvas.cpp and
  mapcanvas_gl.cpp passed generated-command syntax/type checks.
- TestTerrainMovementDiagnostics compiled/linked and passed under ASAN/UBSAN.
  Covers disabled silence, early pre-matching paint, repeated requests, flicker
  overlap, rejected attempts, coalescing, one trace per frame, FPS settings changes,
  and operation with lights off.
- Existing TestTerrainRoomLighting and TestTerrainLightVisibility passed again.
- Formatting/whitespace and both supported installers checked. Dry run, apply,
  idempotence, exact hashes, rollback/reapply and rejection of mismatches passed.

No new full app link or Mac runtime test was performed here. Use the normal Qt
Creator build and supply the traces for the next diagnosis.
