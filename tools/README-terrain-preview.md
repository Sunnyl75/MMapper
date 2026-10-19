# City terrain review fixture

The fixture links the production app and renderer with a different entry point.
It does not change the shipped MMapper executable or add preview controls.
It creates 252 rooms, six buildings, partitions, an L-shaped building, open
entrances, broad streets, passages, courtyards, and surrounding fields.
The saved map room numbers seed every sprite. Server room IDs are left unknown.

After a Linux Debug Ninja build of `mmapper` with the required ccache launchers:

```sh
python3 tools/build_terrain_city_preview.py
./build/city-preview
```

Run with a desktop display or Xvfb. The program captures to `terrain-preview/`
in its working directory and exits after about twenty-two seconds. It writes a
normal app screenshot, an overview, a close-up, a 5.6-zoom material inspection, and a separate labelled cyan
square-grid diagnostic. The diagnostic grid is painted only onto that image.
All rooms are lit and marked SUNDEATH, deliberately including indoors, to
exercise terrain-based exclusion of solar shadows. The clock receives a MUME
afternoon observation after the OpenGL context has initialized.

## Material conventions

City cobbles and indoor flagstones are square source images. Each occupies a
complete 512-pixel texture-array page, with a periodic edge collar and matching
opposing pixels at every mip level. World coordinates span four rooms per
repeat. Two identical pages preserve the existing shared-family stride.

The V3 eave sheet is a six-column/four-row modular set. Fixed rows
hold six straight strips, six convex hips, six concave valleys, and six flush
end boards. Roles never undergo seasonal reordering. Straight strips have two
staggered courses and repeat in a physical 0.75 by 0.24-room frame; continuous
world phase and a shared variant along each wall line prevent room-boundary
restarts. The same assembly rotates for every orientation. Corners have square
frames with perpendicular shingle faces; narrow end boards are 0.027 rooms wide.
Each valley has one owner; the other perpendicular strip yields its corner.
All pieces remain in their owning rooms.
Separate stone and timber sheets supply exterior and interior faces.

Wall depths are room fractions: timber 0.07, eaves 0.24, exterior stone 0.11,
underground rock 0.20. City gutters add 0.035. Every face stays inside its owning
room. A ten-foot nominal wall in a twenty-foot room projects sky shadows onto
eligible outdoor receivers only. Unsupported or missing wall art retains the
original MMapper wall renderer. Doors retain the original renderer.

Transparent grime extends beyond the visible face and gutter; true wall
corners accumulate more. Narrow walls discard fragments outside their footprint
before sampling, preventing neighbouring atlas cells leaking through mipmaps.

Free ends are detected from matching collinear neighbours, so straight runs do
not gain a post at every room boundary. Timber gets a bevelled end-grain post,
stone gets an authored square pier cap, eaves get their matched end boards and hip/valley pieces, and cave
ends retain their rock material. A convex building corner uses a small return
in the diagonal street/field room, preserving room-local ownership. Roof and stone finishes use a dedicated two-layer shared atlas; there are no additional lighting passes.
Explicit gradients keep rotated caps identical at atlas/mip boundaries.
Decorative roof caps now draw in a separate final batch, after every wall and
eave. `terrain-roof-caps.png` is a 3x2 sheet of three hip and three valley covers.
The importer places them in the first two rows of the existing finishing atlas,
leaving stone pier caps unchanged. Missing artwork clears those slots instead
of substituting the old square roof decorations. This adds one draw call per
layer containing caps, with no extra texture layers or dependencies.

Caps have a 0.045-room overlap around the roof-depth square. Outside caps are
split into three room-owned fragments sharing one source frame: the corner
quadrant and shoulders over the two adjacent straight runs. No fragment enters
the diagonal indoor room. Inside caps overlap both roof faces within their own
outdoor room. These are raised decorative covers, not edge-matched shingles.

Gutters are 0.012 rooms deep with a low-opacity feather on both edges; outside
corner bases no longer draw a square gutter frame. The roof's inner 0.035-room
cut has opaque backing sampled from its own wood texture, covering the retained
four-texel MMapper wall without making the outer shingle tips opaque. Empty art
stays empty. The overlay source and prompt are in `terrain-roof-cap-prompts.md`.

Grime tapers with an irregular envelope at open doorway ends; continuous walls
and actual corners retain their joined bands. Wall shadows use the union of
finite-width swept segments with rounded ends, softened once after union.
This covers corner caps even with sunlight parallel to a wall, avoids doubled
shadow opacity, and feathers the silhouette in world units. Tree silhouettes
also receive a small five-tap alpha feather in their existing shadow pass.


## Validation

```sh
cmake --build build --target TestTerrainSprites TestTerrainEdges TestTerrainSpritePacks TestTerrainWalls -j 2
ASAN_OPTIONS=detect_leaks=0 ctest --test-dir build -R '^TestTerrain' --output-on-failure
mkdir -p wall-atlases
ASAN_OPTIONS=detect_leaks=0 ./build/tests/TestTerrainWalls wall-atlases
python3 tests/terrain_edges_render.py gl shader-walls.png wall-atlases
python3 tests/terrain_edges_render.py es shader-walls-es.png wall-atlases
```

The Python shader checks require Pillow, PyOpenGL, and Mesa EGL.
Leak detection is disabled here because the container cannot inspect thread
information; address and undefined-behaviour sanitizers remain enabled.

The reconciled version is based on the preserved bdd0b7b6 checkpoint and retains
its Terrain Preview Clock unchanged. See
`docs/terrain-checkpoint-reconciliation.md` for installation and scatter details.

## Artwork distortion and edge fixes

Squared ashlar replaces rubble. The masonry importer repeats strips at their
natural physical aspect ratio instead of stretching one short strip over a
whole room. Hip, valley, bargeboard and stone pier pieces are separate authored
sprites in `terrain-wall-finishes.png`, with fixed rows rather than seasonal
reordering. Corners use uniform scaling and rotation, without diagonal UV shear.
The optional finish family brings the maximum texture array use to 144 layers.
Missing finish art leaves the finish undrawn; no stretched wall fallback is used.

Wall sampling computes a half-texel guard for the coarser participating mip,
preventing adjacent atlas cells from outlining the eaves. Boundary extrusion
keeps the artwork's ink edge instead of replacing it with a bright sampled line.
The gutter is a recessed channel with a softened outer edge, without the old grid strokes.
The diffuse-shadow and grime logic from the previous update is unchanged.
The private fixture checks the bundled artwork and shader bytes against the
source files before capturing, preventing review of stale resource objects.
The source artwork prompts are recorded in `terrain-wall-art-prompts.md`.
