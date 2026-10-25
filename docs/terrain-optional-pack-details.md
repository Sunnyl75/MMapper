# Optional connected terrain details

Named/manifest packs may include either of these optional six-cell 3x2 PNGs:

- `terrain-road-cart-tracks.png`: straight A, straight B, north/east bend,
  north/south/east T, four-way cross, north-facing dead-end. Top row then bottom.
- `terrain-city-wall-fringe.png`: six horizontal wall-foot strips with alpha
  trailing away from the wall. The loader crops their common vertical band and
  keeps authored width/endpoints. The top of a strip is the wall side.

They are fixed-role artwork, with no seasonal rows. They are resolved per file
through the same ordered resource roots as normal pack art. Absent/invalid art
has no effect. The stable terrain-material family enumeration is unchanged.

The loader appends fixed cells and records optional tile bases in metadata
columns 200/201, outside the 32 family records and seasonal undercoat colours.
Alpha indicates availability. Allocation respects the texture-layer limit and
existing reflection-page reservations. No unavailable optional art is borrowed
from another area.

The road fragment shader chooses a whole motif using the existing NESW road
mask, excluding walls, and rotates it. It samples common straight endpoints at
connected boundaries to hide small authored endpoint differences. It composites
onto the existing full-room earth underpaint. It never changes trail rendering.

Walls retain their explicit tile in the low 20 bits; high bits carry the same
pack selection used by grounds. Optional fringe replaces the narrow procedural
gutter within a 0.065-room band. Wall rotation, room clips and existing end/corner
geometry still apply. Packs lacking fringe keep the 0.012-room gutter.

Details load with terrain textures. No new draw pass, network request, timer,
movement scan or lighting-index update is added. There is extra texture sampling
for detail-bearing roads/walls. Mac GPU performance still requires measurement.
Future packs use these filenames; no area names are present in the renderer.
