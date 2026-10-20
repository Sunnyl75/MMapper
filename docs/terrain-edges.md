> Historical stage documentation. Installation/base and preview-clock statements below describe that stage. For the reconciled checkpoint version, use [terrain-checkpoint-reconciliation.md](terrain-checkpoint-reconciliation.md); do not apply the historical patch commands below.

# Integrated organic terrain artwork

The V6 patch is a full replacement for the saved V5 and edge V1 patches,
against upstream `6a0ae0ec08d3add95c0f0eb425ed04be9f807799`.
It does not contain the later, unavailable local Terrain Preview Clock stash.
Merge it with that work rather than discarding the stash to force application.

Organic terrain now includes seasonal ground and foreground artwork for fields,
forest, mountains, hills, brush, marsh, tunnels and caverns (the SHALLOW sector). Roads and trails
use illustrated dirt/gravel material. Roads are composited into their base
undercoat below neighbouring terrain, allowing adjoining grass, roots and rocks
to fringe the surface naturally. Road rooms do not emit their own overlapping
ground patches, which would hide the road. Trails retain their pass above terrain
and their verge artwork.
Open water and rapids form the bottom material beneath surrounding terrain.
Their detail now comes from six irregular alpha-edged painted water patches
per room, with all six variants represented, independent saved-ID seeding,
random sizes and horizontal flips. A quiet seasonal blue undercoat replaces
the repeated mirrored water image. Patches cross cardinal/diagonal water
neighbours, are clipped outside mapped water, and draw before all land patches.
Overlapping land ground patches and foreground shapes supply the natural
shoreline fringe; separate water-bank strips are no longer emitted. Winter sheets contain snow. Turning seasonal terrain off retains summer.

Saved map room IDs determine layout, size and variant; server IDs are unused.
Foreground instances retain global north-to-south painter ordering. Vegetation
anchors inside connected trail corridors are omitted without refilling their
positions. Roads sit below neighbouring ground and foreground sprites; trails remain above terrain. Ground patches overlap across
land sectors and extend naturally over adjacent water. Directional ground art,
including field grass, stays upright rather than being quarter-turned.
Normal map walls and navigation overlays retain their existing order.

`TerrainMaterials.h` defines the shared family/atlas contract. All artwork uses
the existing instanced texture array and shaders, adding 38 atlas layers after
regional tree packs. The existing maximum of 44 regional tree packs plus the
fallback uses at most 136 layers. No per-sprite draw calls, map format changes,
image downloads or new timers are needed. Shared textures can be overridden
through the existing resource directory mechanism.

Six-column, four-season input sheets are packed into two atlas layers each.
A CPU import step detects four alpha-separated artwork rows and aligns complete
frames using one common scale and root baseline. It prevents misplaced generated
row gutters from cutting off branches or hills, without changing the PNG source.
Opaque road/water materials retain their full cells. Premultiplied resampling and
existing mipmaps remain in use. Road material sampling mirrors in world coordinates. Water uses seeded
overlapping artwork instead of a periodic material.

Neighbour lookup uses actual adjacent positions on the same elevation, not exit
connections. Missing/ambiguous neighbours do not invent banks. Water coverage still follows mapped water rooms, so large river bends can
retain their mapped stepped outline beneath the irregular terrain fringe.
Bridges and inferred river courses are future refinements.
Large foreground shapes may overhang a trail even when their anchor is excluded;
the trail surface is rendered above them.

## Validation

Linux Debug with ccache: the three changed application compilation units compile
and link into the full Qt application; the application runs with the updated Qt
resource bundle and captures the integrated renderer on a synthetic mixed-sector
map. The capture is actual app output, not an illustrated mockup. The preview
harness substitutes only its main entry point to load the fixture and capture it.

Run the C++ regression targets `TestTerrainSprites`, `TestTerrainEdges` and
`TestTerrainSpritePacks`. The latter includes offset-row import checks.
`python3 tests/terrain_edges_render.py gl` and `... es` check the production
shader geometry/continuity/season assertions using synthetic material textures.
With the actual atlas directory argument they each pass 1,431 assertions;
the app capture checks the bundled art itself. Native macOS and the user's
unavailable clock branch have not been tested.

Inspect and check a clean upstream checkout before applying V6:

```sh
git apply --stat /path/to/mmapper-organic-terrain-v6.patch
git apply --check /path/to/mmapper-organic-terrain-v6.patch
git apply /path/to/mmapper-organic-terrain-v6.patch
```

Do not apply this full replacement on top of V5/V1. Merge the changes on an
existing terrain branch and preserve the local clock work.

Roads now occupy about 56% of a room width, versus 15% for trails. Mixed
road/trail joins taper to their shared width and keep the last tenth straight
and constant-width to avoid antialiasing seams.


## Tunnel and cavern materials

Tunnels use lighter warm-grey gravel, compact rubble and worn stone; caverns
use cooler broad limestone slabs, cracks and mineral seams. These are overlapping
floor patches without enclosing walls or drawn room outlines. Twenty-four fixed
all-season variants are available for each floor and for shared small props.
One candidate prop is considered in about one quarter of underground rooms;
trail exclusions may remove it. Most props are small stones, with bones making
up one quarter of the variants. There are no large foreground formations.

The ALL_SEASON instance flag freezes frame selection, using the 24 atlas cells
as permanent variants instead of seasonal frames. Ground and foreground use
the normal room tint; artwork is intentionally mid-light to retain contrast
under MMapper's dark overlay. A separate cave app capture shows tunnels on the
left, caverns on the right, dark rooms above and lit rooms below.


## Wall boundaries

Background undercoats and ground/water patches stop at solid walls, doors drawn
as walls, and absent exits. A 0.025-room recess on each side leaves a visible
break beneath the normal wall line. Both adjoining rooms' flags are consulted,
including rooms outside the viewport; one-sided or conflicting exits cannot
let a patch through a wall. Diagonal overlap is allowed only when both routes
around the corner are clear. Duplicate room positions are treated conservatively.
External unmapped exits without walls retain natural overhangs.

Foreground artwork is never clipped. Its complete quad is fitted inside the
wall planes with a small clearance; blocked diagonal corners try deterministic
alternate positions. Image identity, size and season seed remain unchanged.
Sprites that cannot fit are omitted, and moved anchors are checked against trail
corridors again. Tree shadow projections are clipped at wall planes separately.

The same fixed-size instance buffer is retained. The former spare high bits
of the seed field carry a four-bit wall mask and an eight-bit overlap mask;
low sixteen bits still carry the unchanged seasonal seed. These are rebuilt
with the normal map mesh, not on animation/clock frames. No map-format or public
mesh-structure change is needed. This follows mapped wall/door flags, not a
new live door-open/closed state model.

See `wall-overlays.md` for the separate inward wall-art layer, unsupported-material
behaviour and indoor celestial-shadow exclusion added after the V6 wall clipping.
