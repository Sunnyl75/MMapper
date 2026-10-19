> Historical stage documentation. Installation/base and preview-clock statements below describe that stage. For the reconciled checkpoint version, use [terrain-checkpoint-reconciliation.md](terrain-checkpoint-reconciliation.md); do not apply the historical patch commands below.

# City and indoor terrain

CITY uses continuous grey-brown cobbles, INDOORS uses pale stone flagstones.
Both use the existing texture-array material loader, with no foreground plants.
Their artwork is permanent rather than seasonal. Grounds use continuous world
coordinates, reassembling all 24 cells of each authored paving sheet so open
connections have no per-room texture restart or mirrored-cell motifs.

Solid indoor walls show a narrow exposed timber cross-section. Solid city walls
show wooden shingle eaves only when the mapped neighbour is INDOORS; otherwise
they show a drainage gutter alone. One north-edge shingle strip, with three
staggered courses, is rotated for all directions. Roof tips therefore always
point perpendicular to their supporting wall. Both halves remain inside their
own room. Corners overlap the adjacent rotated strips; no bespoke hip/corner
roof sprite is included in this first implementation. Doors keep the original
MMapper renderer. Missing or invalid wall artwork draws no replacement.

Wall grime is a separate translucent overlay. Continuous irregular bands fade
from real wall bases onto each room's ground, with stronger buildup where bands
meet at a corner. Open exits have no wall band. Grime is independent of sunlight.

City and indoor walls cast sky shadows onto eligible outdoor rooms. Nominal
height is 10 feet at an artistic scale of 20 feet per room, exposed as constants
in TerrainWalls.h. Height is carried in each receiver instance; the shader uses
the existing sky slope (including its low-sun cap of three times height). The
result follows the existing MUME clock and moon model without remeshing on ticks.

Each receiver packs a 5-by-5 neighbourhood's four wall bits into the existing
four integer instance fields (25 nibbles). The shader unions projected segments
before a single alpha blend, so shared walls and intersecting shadows do not
stack opacity. Indoor, underground, underwater, permanently shaded and dark
rooms never get receiver instances. No cross-elevation projection is performed.

The maximum atlas allocation is now 136 layers for 44 regional packs plus the
fallback and 23 shared families, below the GL 3.3 / GLES 3.0 minimum of 256.
Only the first shared tile is packed into eleven bits; later material tile
indices can exceed 2047. Native builds retain Qt's big-resource object path.

## Applying

The incremental city patch applies after mmapper-wall-overlays-v1.patch, from
the previous wall-overlay delivery. The complete terrain patch is an alternative
replacement against upstream 6a0ae0ec08d3add95c0f0eb425ed04be9f807799. Do not
stack the complete patch with earlier terrain patches. The user's separate
Terrain Preview Clock work is not included and must be preserved when merging.

## Validation

Linux Debug build with ccache; terrain sprite, edge, pack and wall tests;
production shader rendering on desktop OpenGL and OpenGL ES, including rotated
walls, missing art, receiver shadow unions and transparent corner grime. The app
capture uses a synthetic map fixture in the production MainWindow and renderer,
with a three-room-wide cobbled road, adjoining indoor floors, a partition and an
open full-room entrance. Clock input is set to an afternoon for visible shadows.

All four C++ targets passed. With atlas exports from TestTerrainWalls, the
production GL and GLES render suites each passed 1,431 assertions. Native macOS
has not been built in this environment. Run the artwork checks with:

```sh
ASAN_OPTIONS=detect_leaks=0 build-walls/tests/TestTerrainWalls /absolute/path/qa
python3 tests/terrain_edges_render.py gl /absolute/path/qa/gl.png /absolute/path/qa
python3 tests/terrain_edges_render.py es /absolute/path/qa/es.png /absolute/path/qa
```

Leak detection is disabled here because the sandbox does not expose the process
thread information LeakSanitizer requires; address and undefined-behaviour
sanitizers remain enabled in the Linux Debug build.

## Artwork provenance

The four new sheets were generated with the built-in image-generation tool.
They are bundled under src/resources/pixmaps as terrain-city-ground.png,
terrain-indoor-ground.png, terrain-timber-walls.png and terrain-city-eaves.png.
The material prompts specify an exact six-column/four-row atlas, permanent
variants, strict overhead view, clear ink accents and subdued earthy painted
Bree Legends comic styling. Ground sheets are opaque; wall sheets use alpha.

City ground: small irregular grey-brown rounded cobbles, subtle wear, no plants,
walls, shadows, gutters or tile labels. Indoor ground: large pale warm-grey
rectangular flagstones, varying dimensions, worn chipped corners and fine
cracks, no props or directional shadows. Timber: 24 narrow exposed cross-section
strips of rough honey-brown beams, grain and joined ends, no posts or cap blocks.
Eaves: 24 north-edge strips of hand-split weathered grey-brown shingles in three
overlapping half-width-staggered courses, vertical grain, roots at top, ragged
tips below, a clean upper transection and narrow stone gutter at the lower edge.
All other wall orientations are rotations of that same source.
