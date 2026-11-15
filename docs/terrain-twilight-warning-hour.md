# Twilight follows one shared visual schedule

Each dawn/dusk colour sequence lasts **exactly one MUME hour**. Dawn uses the
native DAWN hour. Visual dusk starts one hour after the native DUSK warning,
when the native clock enters NIGHT, and reaches full darkness one hour later.
This preserves the evening timing confirmed in November; master's clock,
observer and time classification remain unchanged.

## Single time authority

`MumeClock::getDawnDusk(month)` and `MumeMoment::toTimeOfDay()` already drive the
bottom-right widget. `MumeClockWidget::updateTime()` selects red for DAWN and
blue from DUSK onwards. In the native clock's terminology, DAWN and DUSK are
the one-hour transition/warning phases; their table values mark the START,
not the end, of each native warning. `terrain_twilight::sampleClock()` is the
shared conversion from that table to the visual palette. Rendering, animation
wake-up and integration tests must call it rather than applying their own offsets.

| Native boundary | Countdown indication | Terrain/weather palette |
| --- | --- | --- |
| `dawnHour` | Red warning starts (DAWN) | Start at configured night colour |
| `dawnHour + 30 minutes` | Halfway through warning | Middle of violet/rose/peach/gold sequence |
| `dawnHour + 1` | DAY begins | Full daylight; no night wash or twilight grade |
| `duskHour` | Blue warning starts (DUSK) | Daylight; visual evening has not started |
| `duskHour + 1` | Native NIGHT begins | Start the visual dusk palette at daylight |
| `duskHour + 1 hour 30 minutes` | Native NIGHT | Middle of gold/amber/rose/violet sequence |
| `duskHour + 2` | Native NIGHT | Full configured night wash; twilight grade finished |

The live weather tick checks `sampleClock(...).changing`, using an empty tint
because the animation window does not depend on its colour or opacity. It wakes
the renderer during DAWN and the first native NIGHT hour, even if a clock sync
enters that interval without changing the observer's time-of-day enum. A final
frame is requested when the previous palette was changing, so its endpoint is drawn.
The palette uses that same moment and monthly schedule.
It does not watch pixels, colour styles or countdown strings, and is therefore
independent of clock-widget visibility or language. There is no duplicated
monthly timetable or independent rendering clock.

All existing palette colours, smooth interpolation and night/moon settings
are retained. The complete sequence, including its highlight bell, is fitted
to one hour. For Month 1 the evening runs from 19:00 to 20:00. The first instant equals the preceding stable state and the last
instant equals the following stable state, avoiding a jump at either boundary.
The effect reaches its endpoint at DAY or the end of visual dusk, without a trailing real-time fade.

## Preview and live operation

Preview uses the same time function as live lighting. Its displayed Hour is
00–23 game time. The native bottom-right clock still shows LIVE time; it is
not changed by preview. Test matching times against the selected month's
schedule, rather than comparing a preview hour to the unrelated live clock.

Live animation interpolates within the current game-minute tick, capped at
that minute. Paused preview stays at its chosen minute. Clock synchronization
and precision still follow master. Unsynchronized/unknown live time retains
the existing fallback rather than inventing a warning time.

## Scope and regression coverage

No room lighting rules, live doors, diagnostics, tokens, shadows, artwork,
clock UI, map matching or upstream time code is changed. No new shader,
texture, timer, dependency or mesh rebuild is introduced.

Tests verify exact start/end instants, all palette stops, highlight continuity
and midnight. The Qt preview test checks every minute of all twelve production
monthly schedules through the same `sampleClock()` entry point used to render:
colours change during DAWN and the first native NIGHT hour, each lasting exactly
60 game minutes. The earlier tests sampled an unshifted dusk hour and therefore
did not exercise the evening schedule actually used in production.

The palette itself has not been strengthened: at its halfway point its sky-wash
opacity is 29% of the configured full-night opacity. At the default night alpha
of about 0.35, that is about 0.10, with a separate warm highlight grade. A faint
early evening is distinct from a renderer that has failed to wake.
