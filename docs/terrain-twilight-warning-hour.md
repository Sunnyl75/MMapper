# Twilight follows the native dusk-to-night boundary

Master starts darkening at **DUSK**, when daylight-affected races can safely go
outside. That safety boundary is one game hour before **NIGHT**. The terrain
palette uses that preceding hour for dusk colours and reaches full configured
night exactly when NIGHT begins. It must not add another hour after that event.
Dawn and the native clock, observer, countdown and daylight-safety rules are unchanged.

## Single time authority

`MumeClock::getDawnDusk(month)` supplies the native one-hour warning starts.
`terrain_twilight::sampleClock()` is shared by rendering, animation wake-up and
integration tests, with no extra evening offset.

| Native boundary | Terrain/weather palette |
| --- | --- |
| `dawnHour` | Start at configured night colour |
| `dawnHour + 30 minutes` | Middle of violet/rose/peach/gold sequence |
| `dawnHour + 1` | Full daylight; no night wash or twilight grade |
| `duskHour` | Begin dusk colours; master's earlier dark tint starts here |
| `duskHour + 30 minutes` | Middle of gold/amber/rose/violet sequence |
| Final two game minutes before NIGHT | Brief crossover from final violet dusk to night blue |
| `duskHour + 1` | Full configured night wash; twilight grade finished |

For Month 1, dawn is 08:00–09:00 and dusk is 18:00–19:00. Full night is at
19:00, one hour after master's DUSK darkening. The previous extra offset moved
that endpoint to 20:00 and caused a sun-dark synchronization to restart dusk.

The night-blue crossover uses the same two-real-second duration as master's
weather transition. One real second is one game minute. The last violet dusk
colour is retained until that brief crossover, which **finishes at** the hour
mark. There is no second dark overlay covering dusk and no trailing fade after
the night announcement. Earlier dusk colours and morning timing are retained.

The live tick samples the same visual schedule to wake animation, including
after clock synchronization, and requests one last frame to draw the endpoint.
Moon/night colours and the Time of Day intensity preference retain their roles.
The existing moon-brightness transition remains separate from dusk timing.

## Live time, preview and synchronization

Preview uses the same palette function and monthly schedule as live lighting.
Its Hour field is actual 00–23 game time; the main clock continues to show live
time. Live animation extrapolates only within the latest game-minute tick,
while paused preview stays exactly at the selected minute.

The native `Event.Sun {"what":"dark"}` synchronization sets the clock to
`duskHour + 1`. That instant must sample full night, even if the observer was
already NIGHT and emits no new time-of-day change. Repeated synchronization
must never restart a dusk fade. Unknown live time keeps the existing fallback.

## Regression coverage

Tests check every minute of all twelve production monthly schedules, exact
DAY/NIGHT endpoints, the final violet hold, the two-second crossover,
continuity, midnight wrapping, zero-opacity animation samples and repeated
sun-dark synchronization. Dawn/dusk each occupy exactly 60 game minutes.

The change uses the existing weather draw pass. It does not change room
lighting rules, doors, tokens, shadows, artwork, clock UI, map matching or the
upstream game clock, and adds no shader, timer, texture or mesh rebuild.
