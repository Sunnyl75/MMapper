# Server-observed door appearance

For opt-in on-screen/log measurements and the isolated live-door test switch,
see `terrain-door-performance.md`.

This extension supersedes the static-only rendering in `terrain-static-doors.md`.
It applies incrementally to the current reconciled terrain work and preserves
the rapids/flow, native clock timing, preview clock, light halos and relief
shadow refinements. It can also be merged into the combined token build.

## Behaviour

- A confirmed open door shows only its two posts. There is no swung leaf.
- A closed door uses the existing full artwork. Unobserved doors retain the
  existing decorative marker; this does not assert a remote door is closed.
- Reciprocal adjacent door halves use the same saved-room variant and state.
  One-way doors do not propagate their state into an unrelated neighboring half.
- Only four current-room observations are retained. Movement, a new connection,
  Core.Goodbye and loading another map clear them. A mapped room's server ID must
  agree with the observed room before live state can affect it. If the server
  supplies no usable room identity, its doors retain their decorative markers.
  Sprite variation still uses saved MAP room IDs, never server IDs.
- This affects horizontal door artwork only. Up/down markers and lighting's
  existing wall/door blocking rules retain their current behaviour.

## Notifications and cost

The captured MUME sequence supplies exactly what this requires:

```text
Room.UpdateExits {"n":{"flags":["closed"],"name":"door"}}
Room.UpdateExits {"n":{"id":8682859,"name":"door"}}
Room.UpdateExits {"n":{"flags":["closed"],"name":"door"}}
```

An included exit is a complete replacement description of that exit: an absent
`closed` flag means open. Omitted directions retain their previous states.
An exit value of `false` removes its observation. Malformed data is ignored.

`TerrainDoorState.cpp/.h` isolate the observation logic. MMapper's existing
core GMCP table spells the message `Room.Update.Exits`; our extension also
recognises the actual `Room.UpdateExits` name directly, case-insensitively,
without changing upstream GMCP dispatch or map/path-matching code.

No polling, outgoing requests, text triggers, command interception, timer,
map edits, map scan or geometry invalidation is added. A changed observation
requests an ordinary coalesced frame. One 16-byte integer uniform selects a
frame in the existing instanced door mesh. Matching a reciprocal peer is a
vertex-shader coordinate/flag check. Its adjacency and reciprocal saved-map
links are validated when the normal terrain mesh is constructed.

## Artwork

No new PNG or image-generation step is required. The existing six styles in
`src/resources/pixmaps/terrain-doors.png` already contain the correct posts.
During atlas loading, `alignDoorSheet()` uses the closed row for both states
and clears the space between the post caps for the open frames. This retains
the original post pixels, colour, shared anchor, dimensions and variation.
The old source row containing swung leaves is no longer used for open doors.
If suitable separate post caps cannot be located, no open sprite is invented.
The existing 24-cell atlas allocation, resource list and texture-layer budget
are unchanged. This work is done during texture loading, never per movement.

## Local verification

Build using the same Qt Creator project and kit. No dependency installation or
separate project is needed. Optional `MMAPPER_DOOR_TRACE=1` remains diagnostic
only and is unnecessary for the live appearance.

Log in beside a mapped horizontal door; open it and let it close while staying
in the room. The leaf should disappear and reappear while the posts stay put.
Check each direction, a reciprocal pair, and a one-way door. Terrain meshes
should not be regenerated merely because one of these live states changes.

CPU tests replay the real captured payloads and check duplicate/partial updates,
identity mismatches, movement/reset, removed exits and invalid data. Qt asset
tests verify the exact post pixels and transparent opening across all six
styles. Production shader tests cover all directions, paired versus one-way
doors, elevation mismatches, and changes to uniforms with the same GPU buffer.
See the delivery's `VERIFICATION.md` for the actual platforms and results.
