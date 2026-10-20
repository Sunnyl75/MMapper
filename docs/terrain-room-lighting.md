# Warm terrain room illumination

Base: terrain-only checkpoint `fe369aba780bb01db2647f1027649abd6fed266f`.
No token-manager code, assets, map-format changes, or additional dependencies.
The preview clock, seasonal scatter, live doors and previous terrain refinements remain.

## Controls

Preferences → Graphics: enable Organic terrain and Warm room illumination.
Light strength defaults to 100%; flicker defaults to 35% (a subtle variation).
Set flicker to zero for steady light. These settings persist and repaint without remeshing.
Lighting follows the focused organic-terrain layer and its existing detail zoom cutoff.
It works in daylight, night, and the preview clock's dawn/dusk transitions.
Flame motion follows real elapsed time, including while the preview clock is paused.

## Permanent evidence

* INDOORS, TUNNEL and CAVERN rooms marked LIT receive illumination.
* Otherwise, a static description needs both a fixture and nearby active-light wording.
  Supported sources include torches, lanterns, lamps, candles, chandeliers, braziers,
  fireplaces, hearths, fires, coals, embers, furnaces and sconces.
* Negated, conditional, cold, extinguished and merchandise descriptions are excluded
  conservatively. Names and saved contents are deliberately not scanned: neither proves
  an active fixed light. Static wording remains an inference, not a server guarantee.
* Clear active wording may outweigh a stale DARK annotation. No flags are modified.
* There are no drawn lamp, torch or fire objects. The room itself is illuminated.

The map is scanned once per load. A retained immutable map snapshot is compared when
its revision changes. Only changed description/terrain/light/position/saved-ID inputs
are reclassified; deletion removes cached entries. This still walks room IDs on a
revision, but does not rescan every description. There is no per-frame text scanning.
The derived index is in memory; reopening the map recomputes it rather than storing
stale inferred metadata in the map file.

Each saved map room number produces fixed brightness, colour, phase and speed.
Server IDs do not seed appearance. A 16-frame PNG mask sheet provides soft, moving
corner shadows and a warm centre. Adjacent frames blend on the CPU; each saved room
number determines brightness, warmth, speed and starting frame independently.

## Live artificial light

Char.Vitals light="!" temporarily lights the current room and its character.
It does not establish who owns the light and does not change permanent room evidence.
Missing light fields in partial updates retain the observation; other values clear it.
Movement, connection and Core.Goodbye clear old observations. Room.Info anchors fresh
light to the observed room; known server IDs must match the currently mapped room.
Vitals received before Room.Info are held pending rather than lighting the old room.
The observation never makes a light trail or modifies cached permanent classifications.
Permanent and temporary light in one room use the same bounded light field, not a sum.

## Rendering and cost

The earlier per-fragment procedural flicker method has been removed. A small
512 x 32 PNG sheet contains sixteen mask frames. TerrainLightMask.cpp blends them
on the CPU into 16 x 16 pixel room tiles. The sheet is embedded as a Qt resource;
Pillow is needed only to regenerate the artwork, never to build or run MMapper.

One cached RGBA8 texture covers the bounding box of visible lit rooms on the focused
layer. Its channels are warm red/green, corner-shadow opacity, and illumination.
Allocation is bounded to 256 x 256 rooms (64 MiB); ordinary views are much smaller.
Only that layer is drawn into the mask. A temporary live light overwrites the same
room tile rather than adding a second light. View/evidence/live-light/settings
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
The tile border is explicitly zero for both illumination and shadow, preventing
filtered texture sampling from leaking across walls into another room.

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
