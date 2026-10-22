# Twilight follows the native clock warning

The full dawn/dusk colour sequence lasts **exactly one MUME hour**, starting
when the native countdown enters its warning colour. It no longer spans two
hours. This corrects the palette timing and its animation wake-up condition;
master's clock, observer and time classification remain unchanged.

## Single time authority

`MumeClock::getDawnDusk(month)` and `MumeMoment::toTimeOfDay()` already drive the
bottom-right widget. `MumeClockWidget::updateTime()` selects red for DAWN and
blue from DUSK onwards. In the native clock's terminology, DAWN and DUSK are
the one-hour transition/warning phases; their table values mark the START,
not the end, of each transition.

| Native boundary | Countdown indication | Terrain/weather palette |
| --- | --- | --- |
| `dawnHour` | Red warning starts (DAWN) | Start at configured night colour |
| `dawnHour + 30 minutes` | Halfway through warning | Middle of violet/rose/peach/gold sequence |
| `dawnHour + 1` | DAY begins | Full daylight; no night wash or twilight grade |
| `duskHour` | Blue warning starts (DUSK) | Start at daylight |
| `duskHour + 30 minutes` | Halfway through warning | Middle of gold/amber/rose/violet sequence |
| `duskHour + 1` | NIGHT begins | Full configured night wash; twilight grade finished |

The live weather tick checks `moment.toTimeOfDay()` for DAWN/DUSK, the same
classification used by the clock's colour notification. It requests animation
inside those phases. The palette uses that same moment and monthly schedule.
It does not watch pixels, colour styles or countdown strings, and is therefore
independent of clock-widget visibility or language. There is no duplicated
monthly timetable and no extra one-hour offset before the native warning.

All existing palette colours, smooth interpolation and night/moon settings
are retained. The complete sequence, including its highlight bell, is fitted
to one hour. The first instant equals the preceding stable state and the last
instant equals the following stable state, avoiding a jump at either boundary.
The effect reaches its endpoint at DAY/NIGHT, without a trailing real-time fade.

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

The existing tests now verify exact start/end instants, all palette stops,
highlight continuity and midnight. The Qt preview test compares every minute
of all twelve production monthly schedules against `MumeMoment::toTimeOfDay`:
colours change only during DAWN/DUSK, each lasting exactly 60 game minutes.
The guarded installer is incremental to the earlier clock-sync correction,
including its test-variable rename fix. It preserves unrelated door and
performance patches, whether already applied or built afterwards.
