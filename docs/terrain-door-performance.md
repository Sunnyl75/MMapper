# Door and movement measurements

This incremental diagnostic extension goes on top of the accepted live-door
patch. Door artwork, state lifetime, paired/one-way behaviour and the existing
terrain, lighting, clock and token code retain their behaviour by default.

## On-screen view

Enable the existing **Show Performance Stats** option. No environment variable
is needed for the new on-screen measurements. Turn it off and on to reset the
display's averages, maxima and room-count buckets.

The panel adds:

- Current saved-map room number and known door count, split into horizontal
  and up/down doors. Counts come from the current room's six saved exit flags,
  not from a scan of other rooms and not just from visible/open GMCP exits.
- Average/maximum CPU milliseconds for the live-door observer handling full
  room information, exit updates and other/reset messages separately.
- Average/maximum CPU time selecting live door frames and submitting the
  existing door sprite batch. Batch submission covers all its door sprites,
  not only doors belonging to the character's room.
- Last door redraw queue wait and request-to-draw-submission time. These are
  shared-frame delays, not costs caused solely by door logic.
- Last processed movement's received-GMCP-to-draw-submission time, plus the
  accumulated door observer CPU work during that arrival.
- Movement samples grouped by **0, 1, 2, ... 6 known room doors**, including
  sample counts and average/maximum delays. Each bucket also shows average
  arrival door-observer CPU time. The individual log records preserve the
  horizontal/up/down split, since only horizontal doors use the new artwork.

Milliseconds here begin after MMapper receives the GMCP event. They do not
include the time from pressing a key to MUME replying. A late draw submission
is also not a measurement of when the display actually presents the frame.
Rooms without a matched current map room are unclassified rather than being
incorrectly counted as zero-door rooms. Coalesced/skipped movements retain the
existing latest-movement behaviour; compare routes walked at a normal pace.

The native performance display already calls `glFinish()`, which waits for the
GPU. That upstream behaviour is deliberately unchanged. The new submission
timestamps are taken BEFORE that wait, but enabling the panel can still affect
later frame scheduling. For the cleanest comparison, use logging with the
on-screen display switched OFF. The new diagnostic code introduces no GPU
fences, timers, polling, network requests, map scans or mesh rebuild requests.
With both logging and the panel off, its timing scopes read no clock and room
door counting is skipped.

## Logs and A/B comparison

Quit MMapper before each run. Keep lighting, flicker, zoom and maximum FPS the
same. Let startup/map loading finish before walking the route. Include rooms
with no doors and rooms with one or more doors; retrace the same route several
times in both runs. Door presence is a comparison category, not proof that a
door causes a delay. The test switch isolates live observation/frame selection
while retaining static doors, terrain, lighting and the rest of the app.

Run 1, live doors ON:

```bash
MMAPPER_TERRAIN_TIMINGS=1 MMAPPER_DOOR_TRACE=0 MMAPPER_DISABLE_LIVE_DOORS=0 \
"/Users/Lewis/Projects/Master15Apr/build-MMapper-Qt_6_8_3_Official-Debug/src/mmapper.app/Contents/MacOS/mmapper" \
2>&1 | tee "$HOME/Desktop/mmapper-doors-on.log"
```

Quit after the route. Run 2, live doors OFF:

```bash
MMAPPER_TERRAIN_TIMINGS=1 MMAPPER_DOOR_TRACE=0 MMAPPER_DISABLE_LIVE_DOORS=1 \
"/Users/Lewis/Projects/Master15Apr/build-MMapper-Qt_6_8_3_Official-Debug/src/mmapper.app/Contents/MacOS/mmapper" \
2>&1 | tee "$HOME/Desktop/mmapper-doors-off.log"
```

The test switch is read once at startup and persists no preference. Launching
normally restores live doors. Repeat ON/OFF if results are close, to reduce
network and workload variation. Send both Desktop logs for comparison.

Look for:

- `[terrain-door-room]`: one processed arrival, map room number, total and
  horizontal/up/down counts, live mode, door observer CPU, selection/draw-submit
  CPU and received-GMCP-to-submit time. These records precede the stats GPU wait.
- `[terrain-door-bucket]`: summary by room door count, with sample counts and
  averages/maxima. Zero-door rooms have an explicit bucket.
- `[terrain-perf]`: separate `door-observe-room`, `door-observe-exits`,
  `door-observe-other-reset`, `door-frame-selection`, `door-draw-submit`,
  `door-request-to-frame-start`, `door-request-to-draw-submit` and
  `matched-gmcp-to-draw-submit` measures. CPU values use six decimal places in
  logs so sub-millisecond work does not disappear through rounding.
- Existing `[terrain-move]` records retain their previous interpretation and
  append `door-request=0/1` to show whether a door redraw shared that frame.

On-screen sampling does not itself enable console logging. Logging remains
opt-in through `MMAPPER_TERRAIN_TIMINGS=1`. No game commands are sent by either
diagnostic mode.
