# Road / water shoreline sprites

This refinement replaces the initial and stronger procedural road/water fringe
with authored rounded border sprites. Apply it after both road-water-edge and
road-water-edge-v2, leaving the earlier patches installed as its code base.
Do not reapply old cumulative terrain or lighting packages.

## Artwork and placement

`terrain-road-shores.png` is a true-alpha 1536x1024 sheet of 24 fixed variants
in a 6x4 grid. Each is a compact earth/gravel bank, with a fairly smooth road
side, broad stone lobes on the water side, and rounded ends. There are no
seasonal rows: silhouettes stay fixed, while the sampled road material follows
the map's existing seasonal transition. The sheet was created with the built-in
image generator; the full prompt is recorded in terrain-road-shore-artwork.md.

The loader finds each cell's alpha bounds and fits it into a padded frame.
Each unblocked ROAD/WATER edge gets one quad, chosen from the saved map room
number and side. The 1.36-room span extends 0.18 beyond each endpoint. Its depth
runs from 0.14 inside the road to 0.40 inside water. Transparent rounded tips
occupy less than that envelope. The ends overlap adjacent pieces, including
outward bends, rather than ending at a room-sized square corner.

Extensions are allowed only into mapped ROAD/WATER rooms, using the existing
wall-aware overlap mask. Walls on either side block them; diagonal extensions
require both routes around the corner to be clear. Unknown space and other
terrain are clipped. CITY quays remain outside this change.

## Rendering

Reuse bank kind 4 and its ROAD_SHORE role bit. Fixed artwork supplies the alpha
silhouette and restrained dark stone detail near the water-facing edge. The
opaque road-facing portion samples the identical seasonal road material as
the base, at the same world coordinates, without a pale separately painted band.
Mip-aware sampling guards atlas-cell boundaries.

The existing ground batch orders water patches first, shore strips next, then
land fringes. Foreground and wall overlays remain above them. The old
procedural road-shore wave has been removed. Missing artwork draws no strip.
No new timer, light-index rebuild, map update or rendering pass is introduced.
Up to four static strip instances replace the previous single bank instance
per qualifying water room; the family adds two texture-array pages. Mac GPU
cost has not been measured.

Lighting source detection, dawn/dusk, whole-sprite dark-room tint and native
map/highlight updates remain unchanged.

## Validation

Linux Debug/ccache sprite-pack tests cover padded framing and missing artwork.
The changed production drawer and texture loader passed C++ syntax checks;
existing upstream GCC warnings were demoted for those checks. Production
shaders pass desktop OpenGL and OpenGL ES tests using the authored silhouettes,
including all variants, road colour matching, overlap, outward corner coverage,
wall clipping and clear central water. Installer checks cover dry-run,
application, repeat, reversal, unrelated edits, mismatched source and partial
updates. No full application build or macOS visual verification was performed.
