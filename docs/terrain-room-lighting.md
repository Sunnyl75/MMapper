# Warm terrain room illumination

Base: terrain-only checkpoint `fe369aba780bb01db2647f1027649abd6fed266f`.
No token-manager code, assets, map-format changes, or additional dependencies.
The preview clock, seasonal scatter, static doors and previous terrain refinements remain.
See `terrain-room-light-halos.md` for circular pools, wall clipping and street-lamp rules.

## Controls

Preferences → Graphics: enable Organic terrain and Warm room illumination.
Light strength defaults to 100%; flicker defaults to 35% (a subtle variation).
Set flicker to zero for steady light. These settings persist and repaint without remeshing.
Lighting follows the focused organic-terrain layer and its existing detail zoom cutoff.
It works in daylight, night, and the preview clock's dawn/dusk transitions.
Flame motion follows real elapsed time, including while the preview clock is paused.

## Permanent evidence

* LIT alone does not establish an artificial source: it may describe sunlight.
  This applies to every sector, including indoors, tunnels and caverns.
* Otherwise, a static description needs both a fixture and nearby active-light wording.
  Supported sources include torches, lanterns, lamps, candles, chandeliers, braziers,
  fireplaces, hearths, fires, coals, embers, furnaces, forges, smelters, firepits and sconces.
* Street-lamp mentions illuminate enclosed rooms all day; outdoors they illuminate
  during dawn, dusk and night, ignoring their reported on/off wording.
* Negated, conditional, cold, extinguished and merchandise descriptions are excluded
  conservatively. Names are not scanned; saved contents are checked only for street-lamps, not
  portable lights or transient fires. Static wording remains an inference, not a server guarantee.
* Clear active wording may outweigh a stale DARK annotation. No flags are modified.
* There are no drawn lamp, torch or fire objects. The room itself is illuminated.

The map is scanned once on the first active detailed lighting view after a load,
new map or merge. During play, only the room reported by MMapper's processed
movement/room notification is reclassified. Map revisions and flicker do not scan
the map. Other rooms keep their cached illumination until entered or reloaded.
The derived index is in memory; reopening the map recomputes it rather than storing
inferred metadata in the map file. See `terrain-lighting-current-room.md`.

Each saved map room number produces fixed brightness, colour, phase and speed.
Server IDs do not seed appearance. A 16-frame PNG mask sheet provides soft, moving
corner shadows and a warm centre. Adjacent frames blend on the CPU; each saved room
number determines brightness, warmth, speed and starting frame independently.

## Live artificial light

Char.Vitals light="!" temporarily lights the current room and its character.
It does not establish who owns the light and does not change permanent room evidence.
Missing light fields in partial updates retain the observation; other values clear it.
Movement invalidates the room anchor but retains the latest incremental light value.
The next Room.Info attaches it to the destination; known server IDs must match the
currently mapped room. A changed light field always supersedes the retained value.
Connection and Core.Goodbye clear both the anchor and light value.
Vitals before Room.Info update the latest state without lighting an unidentified room.
The observation never makes a light trail or modifies cached permanent classifications.
Permanent and temporary light in one room use the same bounded light field, not a sum.

## Rendering and cost

The earlier per-fragment procedural flicker method has been removed. A small
512 x 32 PNG sheet contains sixteen mask frames. TerrainLightMask.cpp blends them
on the CPU into 32 x 32 pixel circular halos at 16 pixels per room. The sheet is embedded as a Qt resource;
Pillow is needed only to regenerate the artwork, never to build or run MMapper.

One cached RGBA8 texture covers the bounding box of visible lit rooms on the focused
layer. Its channels are warm red/green, corner-shadow opacity, and illumination.
Allocation is bounded to 256 x 256 rooms (64 MiB); ordinary views are much smaller.
Only that layer is drawn into the mask. A temporary live light replaces a permanent source at the same position.
Overlapping pools use the stronger illumination per pixel rather than adding light. View/evidence/live-light/settings
changes invalidate the cache. With zero flicker, time alone cannot invalidate it.

A separate 84 ms QTimer requests at most twelve lighting updates per second. Lighting
no longer registers a continuous-animation callback with FrameManager. The timer
stops when lighting is disabled, steady, zoomed out, or no qualifying light is visible.
It does not request redraws while the application is hidden or suspended.
Inactive-but-visible windows can still animate when using an external MUD client. Other map animations
can still request frames; lighting does not lower their frame rate. Each flickering
mask is uploaded at most once per 84 ms time bucket, even if other effects draw faster.
A zero-flicker view needs no lighting-driven timer or recurring upload.

The shared shader now performs one filtered texture lookup, without trigonometry.
Room multipliers fade toward neutral, night/twilight opacity falls inside light pools,
and the final pass adds warm light plus soft corner darkening. These corner shadows
are decorative, not a simulation of individual furniture or light fixtures.
Cloud, fog, precipitation, map safety markings and existing shadows remain separate.
Wall clipping uses cached room boundaries on both sides of an edge; static doors
also block light. An inward zero guard prevents linear filtering from leaking
through a blocking edge. Open exits allow the circular halo into mapped neighbours.

Screen-to-map projection is calculated at the three fullscreen-triangle vertices,
then interpolated homogeneously, rather than reconstructed for every fragment.
This supports Retina/render scaling and tilted/rotated views without extra pixel
matrix multiplies. It remains a focused-plane effect, not depth-aware transport
through overlapping elevations. Other elevations retain their own tint/fade.

This removes known costly work, but desktop Linux shader tests do not establish
macOS GPU usage. Compare the same stationary scene with lighting disabled, steady,
and flickering on the user's Intel Mac before accepting the performance result.

## Build support

The patch also sanitizes Git-derived package filenames, matching the Mac packaging fix
already applied on the separate combined terrain/token branch. An existing Linux Clang
portability error in AdventureSession::formatPoints is corrected with <cmath>/std::abs.
These small fixes are unrelated to light classification and do not import token work.

## Checks

TestTerrainRoomLighting covers evidence, negation, unknown flags, deterministic variation,
partial GMCP updates, either message order, room changes, high server IDs and reset.
room_lighting_render.py checks the actual production shaders on desktop GL/GLES,
including overlay relief, warm daylight colour, cached-mask sampling and layer isolation.
CPU tests cover steady stability, independent animation and hard wall containment. Existing terrain and twilight
shader regressions must also pass with the shared helper and lighting disabled.

## Delta-light movement fix

The supplied gameplay log has one Char.Vitals light="!" at login and no repeated
light field during subsequent moves. The observation retains that value until the
server supplies a different value, while Event.Moved removes only the previous
room anchor. Room.Info supplies the new anchor, and a mapped-marker update always
requests a redraw, even if the camera did not change and flicker is zero.
The destination gets a fresh cached tile immediately; animation does not govern
movement latency. Room illumination still describes observed artificial light,
not proof of which character or fixture owns it.

Full flicker also now plays the same atlas faster and uses a per-room CPU sample
to strengthen brightness variation. The 84 ms update interval remains unchanged.
At zero flicker, the atlas and brightness remain time-independent. No sprite
artwork, GPU shader, map flags, or permanent-room classification changes.
