# Smooth twilight and calendar-aware preview controls

Apply after mmapper-door-refresh-fix. Required source tree: 25f231b0469b33e1c2f27572b80d19f63105056c. This is one incremental patch, not another cumulative terrain replacement. The same source may be committed or uncommitted; Git synchronisation is not required.

## Dawn and sunset

The existing time-of-day overlay receives one clock-driven colour uniform. In the last MUME hour before sunset, its colour/opacity moves through clear daylight, faint gold, amber, rose, violet and the existing configured night colour. In the last MUME hour before sunrise, it moves through night blue, violet, rose, peach and pale gold into transparent daylight.

Each palette segment uses smoothstep interpolation in linear-light, alpha-weighted colour space. The composited result is continuous at the palette stops, the start/end of each hour, sunrise, sunset and midnight. Live time interpolates between MUME-minute ticks using monotonic elapsed time (one real second per MUME minute), capped at the next tick. It animates without uploading terrain geometry. Paused preview time stays at the selected minute.

MumeClock::getDawnDusk supplies the event times for the selected month. Existing night and moonlit-night colours, night alpha and weather time-of-day intensity setting remain the endpoints/strength controls. Live moon-brightness changes retain a short transition. When live clock information is unknown, the previous named-colour overlay path remains available. Preview time also controls this overlay, in addition to existing terrain seasons/shadows; hiding/disabling preview returns to live time. Preview never changes GameObserver or the saved map. Precipitation/weather observations continue to come from the live game.

The existing time-of-day mesh/draw pass is reused. No new artwork, weather UBO layout, map-format field, external dependency or terrain rebuild trigger is added. Palette code is isolated in display/TerrainTwilight.h.

## Preview clock

The preview widget is extracted from MainWindow into mainwindow/TerrainPreviewClock.cpp/.h. Its location, title, controls, initial date, 1–24 hour display and paused/advancing modes remain familiar.

Arrow, wheel and page stepping use the existing MumeMoment calendar in both directions. Minute 59→0 carries an hour; hour 24→1 carries a day; day 30→1 carries a month; month 12→1 carries a year. Reverse stepping borrows through the same fields. Larger steps and timed advancement use the same normalisation path. All controls update atomically, followed by one preview callback, so intermediate dates are not rendered.

Typing/programmatic setValue edits a field directly and does not infer a carry just because its old/new values are endpoints. The existing year range MUME_START_YEAR–4099 is retained; whole instants clamp at its ends rather than wrapping the year range.

## Validation

Three focused Linux Debug tests passed: existing TestClock, new TestTerrainTwilight and actual offscreen Qt TestTerrainPreviewClock. Palette checks sample each second of the transition for multiple dawn/dusk schedules; test opacity direction, finite/bounded values and visible continuity at every knot and midnight. UI tests cover forward/reverse carries, a minute spanning every year boundary, page steps, year limits, direct edit semantics, one callback per step, timed advancement and restoring live mode on hide.

Four production Clang 18 syntax checks passed: opengl/Weather.cpp, opengl/legacy/Shaders.cpp, display/mapcanvas.cpp and mainwindow/mainwindow.cpp. The new widget also compiled/linked in its Qt test. Builds used project Debug warning/sanitizer flags and ccache; container leak detection was disabled.

The production time-of-day shader passed 10 assertions each on Mesa desktop OpenGL 4.5 and OpenGL ES 3.2, exercising uniform-only colour/alpha changes, transparent daylight and legacy unknown-clock fallback. The terrain/door shaders are unchanged by this increment.

Installer check/application/repeat detection and exact forward/reverse tree checks are recorded in verification.json. No complete application link, macOS build or live gameplay test was performed. The prior door-refresh fix is retained; this patch does not force map-mesh refreshes.
