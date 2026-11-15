# Smooth twilight and calendar-aware preview controls

This terrain extension uses the existing clock and its monthly schedule as the
sole time authority. Current twilight timing is documented in
[terrain-twilight-warning-hour.md](terrain-twilight-warning-hour.md).

## Dawn and sunset

The native countdown changes colour when `MumeMoment::toTimeOfDay()` enters
DAWN (red) or DUSK (blue). Each is exactly one game hour long. The names in the
monthly table refer to the START of those warning phases: DAY begins at
`dawnHour + 1`; NIGHT begins at `duskHour + 1`.

Morning moves from configured night blue through violet, rose, peach and
pale gold, reaching clear daylight exactly at DAY. Evening moves from daylight
through gold, amber, rose and violet during the native DUSK hour, reaching full
configured night exactly at NIGHT. Master already starts its dark tint at DUSK
for daylight safety; the terrain night endpoint is one hour after that earlier
darkening, not another hour after NIGHT. Both live and preview rendering use
`terrain_twilight::sampleClock()` with no extra offset. The last violet dusk
colour is held until a brief two-real-second crossover ending at NIGHT, so
night blue does not gradually cover the preceding dusk colours.
Highlight grading peaks midway through each visual transition and reaches zero at
both ends. The earlier two-hour palette is superseded.

Each palette segment uses smoothstep interpolation in linear-light,
alpha-weighted colour space. Visible colour and opacity are continuous at the
palette stops, warning starts, DAY/NIGHT boundaries and midnight. Live time
interpolates between MUME-minute ticks using monotonic elapsed time, capped at
the next tick. One real second is one game minute. Paused preview time stays
at its selected minute.

`MumeClock::getDawnDusk` supplies the monthly schedule. The native clock widget
still uses `MumeMoment::toTimeOfDay`; the rendering wake-up uses the same
`sampleClock()` visual schedule as the palette, including its exact endpoints.
Night and moonlit-night colours, night alpha and the Time of Day intensity
preference retain their roles. Unknown live time retains the earlier named
colour fallback. Preview uses the same schedule and palette, without changing
GameObserver or the saved map. Hiding/disabling preview returns to live time.
The bottom-right clock continues to show live time while preview is enabled.

The existing weather draw pass is reused. No map rebuild, new timer, asset,
clock-rule change or external dependency is introduced.

## Preview clock

The preview widget is extracted from MainWindow into mainwindow/TerrainPreviewClock.cpp/.h. Its location, title, controls, initial date, standard 00–23 hour display and paused/advancing modes remain familiar.

Arrow, wheel and page stepping use the existing MumeMoment calendar in both directions. Minute 59→0 carries an hour; hour 23→00 carries a day; day 30→1 carries a month; month 12→1 carries a year. Reverse stepping borrows through the same fields. Larger steps and timed advancement use the same normalisation path. All controls update atomically, followed by one preview callback, so intermediate dates are not rendered.

Typing/programmatic setValue edits a field directly and does not infer a carry just because its old/new values are endpoints. The existing year range MUME_START_YEAR–4099 is retained; whole instants clamp at its ends rather than wrapping the year range.

## Validation

`TestTerrainTwilight` checks exact one-hour windows, palette endpoints,
monotonic opacity, finite colours, the highlight envelope, visible continuity
and midnight wrapping. `TestTerrainPreviewClock` exercises the Qt controls,
calendar carries/borrows and the actual production clock schedule. For every
minute of all twelve months, twilight changes during the native DAWN and DUSK
hours: exactly 60 game minutes for each. Full night is present at the exact
NIGHT boundary, including after a repeated sun-dark clock synchronization.
A maximum-summer evening finishes at 23:00 and stays full across midnight.
The raw palette also supports a transition ending at midnight.

See the delivery verification notes for checks actually run and platform limits.

## Preview hour alignment

The previous Hour control subtracted one before creating a MumeMoment. It now
uses the actual displayed 00–23 hour directly. For example, 08:30 previews 08:30.
Date carries and reverse borrowing remain atomic. The preview checkbox tooltip
explains that the main MMapper clock continues to show live game time.

The Qt regression compares all twelve months with the actual
`MumeClock::getDawnDusk` table and `MumeMoment::toTimeOfDay` implementation.
Each complete spectrum fits into one visual transition hour and reaches
daylight or full night at its end. For Month 1, dawn runs from 08:00 to 09:00,
and visual dusk from 18:00 to 19:00, with full night at 19:00.
