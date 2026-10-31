# Shared known-room tracking

`GameObserver` owns one `KnownRooms` instance per connection. It receives GMCP
`Room.Known.List`, `Room.Known.Add`, `Room.Known.Updated` and the current-room
`Room.Info` message, and publishes cheap
immutable snapshots through `getKnownRooms()` and `sig2_knownRoomsChanged`.

Display features register a stable, unique consumer name with
`setKnownRoomsConsumerEnabled(name, enabled)`. Registering a second consumer
does not request another history or create another cache. Removing one
consumer leaves tracking enabled for the others. A consumer must unregister
when disabled or destroyed. The foundation contains no red-dot or fog-of-war
display setting.

`Snapshot::isKnown(serverId)` accepts positive evidence from `Room.Info` and
`Room.Known.Add` immediately, even while history is loading. Only
`Snapshot::isUndiscovered(serverId)` requires a completed history: an incomplete
transfer is not evidence that a room is unknown. Both require a valid server ID.
Entered rooms survive stale list/cache responses. Never infer discovery from
the player's route, map presence, room coordinates or another character.

The existing versioned cache remains under the application data directory's
`known-rooms` folder, separated by server and case-folded character identity.
A completed cached history is usable while the server refreshes it. Partial,
malformed and interrupted replacements cannot overwrite a valid cache. Live
updates received during loading are retained. Character/server generations
prevent stale worker results from applying to a different session.

The proxy sends the sole history request. Renderers consume snapshots only;
they must not parse GMCP, request histories or write parallel caches. Rendering
and cache work remains asynchronous using the existing Qt and standard C++
facilities, without platform-specific dependencies.

This foundation is extracted from the existing undiscovered-room work. It can
be merged before either display feature. The red-dot and fog-of-war changes
then depend on this common foundation, independently of one another.
