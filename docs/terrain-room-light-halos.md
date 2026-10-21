# Circular room-light halos and street lamps

This increment follows the cached-mask lighting, current-room index, movement
diagnostics and twilight-highlight patches. No token code or map format changes.

## Shape and walls

The 16-frame PNG atlas now holds circular pools, not square room masks. Each halo
is 32 x 32 pixels at the existing 16-pixels-per-room density: its centre is the
room centre, with a soft animated rim reaching about a quarter to a third of a
room beyond each side. Corner shadows remain inside the source room and animate
there, so a closed room still flickers even though its exterior halo is cut off.

Every solid NESW boundary blocks, including a wall recorded on only one side.
Doors use the existing static closed-door policy and block light. Open exits let
light through into mapped neighbours. Unknown neighbours are solid. Straight
rays are clipped against the local 3 x 3 neighbourhood; light does not bend around
a corner or route through a different doorway. Rays exactly through a diagonal
corner require both routes to be open, avoiding pinholes. A zero strip just inside
a blocking edge prevents bilinear texture sampling from leaking across it.

Overlapping lights use maximum illumination per pixel, not additive brightness.
A character's temporary light replaces any permanent source at the same position.
The focused layer, masks for removing night/dark overlays, and character-light
behavior are unchanged. The effect still uses the same shaders and render passes.

## Evidence and street-lamp schedule

Recorded LIT flags alone do not qualify in any sector: sunlight can produce
that flag. The later dark-room/sprite patch removes both earlier LIT fallbacks.
Descriptions can identify active forges, furnaces, smelters and firepits as well as
the earlier fixtures. Cold/unused/extinguished fixtures do not qualify by themselves.
Negation is checked near each fixture rather than rejecting a whole sentence just
because it mentions unrelated cold stone or empty furniture. This is a heuristic;
room descriptions remain evidence rather than guaranteed current game state.

Street-lamp, street lamp and streetlamp, including plurals, deliberately use a
simpler rule: always on in enclosed sectors; outdoors on during MMapper's native DAWN, DUSK and NIGHT states. They are off during DAY. Their textual on/off state is ignored as requested. Another active
permanent source in the same room can still qualify it for all-day illumination.
Names are not scanned. Saved contents are checked only for street-lamp mentions;
other mutable item descriptions do not establish permanent illumination.

The existing game clock and preview clock both control the schedule. When clock
time is unavailable, DAWN/DUSK/NIGHT observer states enable outdoor street lamps;
UNKNOWN leaves them off. No new lamp-state parser or polling is introduced.

## Cache and cost

There is still one initial map scan and subsequent classification of only the
entered room. That same initial scan stores the wall flags of all rooms. Entering
a room updates its flags and invalidates only the nine possible source clips that
depend on it. Wall clips are built lazily and cached; flicker does not ray-test or
look up rooms in the map. It samples the atlas, applies cached bits and composites
halos into the existing mask. Runtime memory includes one wall byte plus container
overhead per room and 128 bytes plus overhead per cached source clip.

Street-lamp time changes filter cached entries and invalidate the visibility
cache once at the schedule boundary, without scanning descriptions. Off street
lamps do not keep the lighting redraw timer running by themselves. The existing
84 ms update interval and zero-flicker caching remain. Visible bounds include the
halo, so an off-screen source can illuminate the edge of the view.

The larger masks require more CPU work and somewhat more upload area. A Linux
Debug microbenchmark with 40 lights over 200 masks measured about 1.7 ms for the
new mask construction versus 0.8 ms previously. This excludes the GPU, first-time
clip construction, other map drawing and network latency; it is not a Mac frame
measurement. Existing diagnostics should be used for the Mac comparison.

## Review

Enable warm illumination and use 100% flicker for the first visual check, then
choose a comfortable setting. Inspect an open area, a closed room, one open exit,
a corner, and two neighbouring light sources. Try outdoor street lamps at midday,
just before sunset, night and sunrise with the preview clock. Reload the map once
after building to rebuild the evidence index with the new forge/street-lamp rules.
