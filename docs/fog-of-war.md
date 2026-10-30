# Fog of war review build

Enable **Preferences → Fog of War → Show only rooms known by the current
character**. Open the foreground game map normally and keep the separate
terrain file in **Preferences → Background Map**. Connect to MUME and select
your character. The option defaults to off.

Only rooms with a valid server ID present in the current character's usable
`Room.Known` snapshot are drawn. Before that history is available, the canvas
shows the terrain background and waits for history. A completed cache from the
same character and server is usable while the server refreshes it. Room
presence in the map, movement and coordinates never establish discovery.

Known-room terrain, objects and ordinary room details use the normal foreground
renderer. Connections and door names require visible endpoints. Group markers
in hidden rooms are omitted, and a queued-path overlay stops at its first hidden
room. Hidden rooms cannot be selected through the canvas's room-selection
signal. Existing selections are cleared when a character/history change could
make them invalid.

Free-standing infomarks have no server ID and are hidden in this first version,
along with mapping-status highlights and connection/infomark selection. Turn
fog off to edit connections or annotations. This is a display mode: it does not
delete rooms, change save/export contents, or restrict the underlying route
finder. It is not an access-control mechanism for a loaded map.

## Shared tracking and independent features

The sole observer, GMCP parser, request and cache come from
[the shared known-room foundation](known-rooms.md). Fog registers the named
consumer `fog-of-war`; the red-dot adapter registers `undiscovered-rooms`.
Registering either consumer keeps the common tracker active. Disabling one
does not disable the other. Renderers consume immutable snapshots only.

The development branches are:

- `feature/known-rooms-foundation`: generic tracking, protocol, caching and tests;
  no red-dot or fog display setting.
- `feature/undiscovered-room-dots-shared`: the existing red-dot presentation
  adapted to that foundation, without a dependency on fog or background maps.
- `feature/fog-of-war`: this display mode, combining the existing background
  checkpoint and the shared foundation. It contains no red-dot presentation.

Merge the common foundation once, then review the display changes separately.
The original red-dot PR branch and the working background checkpoint are
preserved. The two display branches use the same cache format and directory.

## Rendering and cache boundaries

An immutable `RoomVisibility` snapshot is captured with each existing async
foreground mesh job. Room grouping and connection construction filter against
it before producing meshes. At most one foreground mesh job runs at a time;
new changes coalesce into the next job.

`Snapshot::revision` identifies any knowledge change. Its `epoch` changes when
knowledge may shrink, and its `generation` identifies character/session changes.
Already visible geometry stays drawable during additions. An incompatible
history or character change hides old geometry immediately; stale results are
drained without a GPU upload. Incomplete or empty histories skip the full room
scan and produce an empty foreground.

The boundary-shadow worker uses the same visibility as the displayed
foreground. Hidden rooms neither cast shadows nor suppress a visible room's
exposed boundary. Changes to knowledge invalidate foreground/shadow work only;
they do not reload, reclassify, retexture or rebuild the background terrain.

This first version uses master's existing foreground batching. A reveal can
rebuild foreground batches asynchronously; it does not yet update individual
foreground chunks. Full-map live-play latency still needs measurement. There
is no new platform-specific rendering, storage or threading dependency.

## Validation

- Linux Debug application build: Clang 18, Qt 6.4.2, ccache, ASan and UBSan.
- `TestProxy`: protocol, malformed/interrupted responses, identity/cache
  isolation, live updates, immutable snapshots and multiple shared consumers.
- `TestFogOfWar`: strict visibility, missing IDs, immutable source maps,
  growth/replacement/character transitions, safe geometry reuse and preferences.
- `TestBackgroundMap`: existing terrain/cache tests plus visible-only shadows.
- Changed C++ files checked with clang-format 18 directly; Docker was unavailable.
- Native macOS, Windows, WebAssembly, live MUME and full-map visual/performance
  review remain pending. The offscreen Linux platform could not create the
  required OpenGL context, so this is not a claim of a native visual test.

Tests use Qt's offscreen platform and disable leak detection for Qt's static
plugin allocations; address/undefined-behaviour errors remain fatal. Optional
bundled media, updater, keychain, OpenSSL and WebSockets were disabled for the
Linux build. These switches are not requirements for the feature.
