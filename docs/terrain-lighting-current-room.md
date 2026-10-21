# Initial index plus entered-room lighting updates

This incremental patch follows the light-follow, timing-diagnostics and visibility
cache patches. Static and animated door versions are supported. No new artwork,
shader, dependency, project or build directory is required.

## Behaviour

The old lighting-index update walked every room whenever the map snapshot changed.
Diagnostics measured about 360 ms per update during movement. This patch removes
that revision-based scan and the retained snapshot used for those comparisons.

- The first active, detailed lighting view builds the permanent-light index once.
- The existing canvas `slot_dataLoaded()` resets that initialization flag and the
  cached lights for a new/opened/merged map. The next active view rebuilds once.
- The existing `slot_moveMarker()` first lets MapData select the room, then passes
  only that room ID and the current map to `updateCurrentRoom()`.
- PathMachine emits that notification after scheduling/applying parsed room changes.
  Lighting consequently reads the mapped room data rather than prematurely reading
  the earlier raw GMCP message. A same-room look/refresh that emits the same room
  notification also checks that one room.
- Every notification is processed immediately, rather than retaining only the last
  ID for a later frame. Rapid movement through several rooms does not skip index
  updates for intermediate rooms.
- Once initialized, an entered room is classified and its single cached entry is
  added, replaced or removed as appropriate. If initialization has not happened yet,
  the callback does nothing: the eventual initial scan sees the latest mapped data.
- If that one entry changes, the small vector of cached light locations is refreshed
  for visibility selection. This copies the light collection; it does not walk or
  classify all map rooms. Unchanged illumination leaves the visibility cache intact.
- Carried-light observation/following, flicker, light masks and drawing are unchanged.

Ordinary map revisions, saves, clock changes and flicker frames never rebuild the
index. Only loading/new/merging a map resets it. No background thread is needed.
Texture work stays on the existing renderer thread. Master's map update, exit
handling, path matching and highlight code are unchanged.

Non-current rooms intentionally retain their cached illumination during play.
Bulk edits, deletions or moves of other rooms are not synchronized automatically;
reload the map to rebuild everything, or enter a changed room to refresh that room.
This patch does not add a manual refresh action.

## Verify on the Mac

Apply using the supplied guarded installer, then Run CMake, Build and Run in the
usual Qt Creator project. Keep `MMAPPER_TERRAIN_TIMINGS=1` enabled.

`light-index` should appear only for initial setup after a map load, not repeatedly
while walking. `light-current-room` reports the subsequent single-room updates,
including any refresh of the cached-light vector. The existing `light-visibility`
and `light-visibility-rebuild` counters remain available. Check both flicker 0%
and normal flicker and confirm that carried light follows the character.

The initial full scan is still synchronous and may pause the first lighting view.
This patch specifically removes repeated full scans during play. Existing native
map comparisons and background highlights remain measurable costs.

## Validation

- Linux Debug CMake configuration with Clang 18/ccache succeeded.
- TerrainRoomLighting.cpp, TerrainLightingDiagnostics.cpp and mapcanvas.cpp passed
  strict production syntax/type checks using generated compile commands.
- Existing TestTerrainRoomLighting and TestTerrainLightVisibility compiled, linked
  and passed under CTest with AddressSanitizer/UndefinedBehaviorSanitizer. These
  cover classification/observation, light masks and visibility regression behaviour;
  they do not simulate a full live-game movement session.
- Reviewed the notification order in PathMachine and all existing slot_dataLoaded
  call sites (new/open/merge). Full-map iteration exists only in the guarded initial
  builder; the current-room callback contains one room lookup and no map iteration.
- Both installer profiles were tested for check/application/idempotence, exact
  resulting hashes, rollback/reapplication and rejection of mismatched sources.
- clang-format 18 and patch whitespace checks passed.

No complete new application link or Mac runtime check was performed here. The
normal Qt Creator build and movement diagnostics are the remaining validation.
