> Historical stage documentation. Installation/base and preview-clock statements below describe that stage. For the reconciled checkpoint version, use [terrain-checkpoint-reconciliation.md](terrain-checkpoint-reconciliation.md); do not apply the historical patch commands below.

# Organic forest terrain — experimental fifth patch

This optional mode draws seasonal forest and field terrain using stable,
overlapping sprites. Forest has trees above moss, humus and fallen leaves.
Fields have low grass beneath spring flowers and summer tussocks. Both ground
types become snowy in winter. No decorative room grid or outline is added;
existing trails, streams, walls, doors, symbols, selections and characters stay
above the terrain.

Enable **Preferences → Graphics → Room Details → Organic terrain (forest and
field sprites)**. **Seasonal terrain** controls trees, ground and field details
together. Organic rendering is off by default; seasonal variation is on.

## Scope

- Forest and field sectors; other sectors retain their normal artwork. Ground
  patches may overhang their edges to soften adjacent terrain.
- Sprites appear on the selected elevation. Other elevations retain original
  artwork/fading. Below zoom 0.4, original forest/field tiles return.
- Sprites lie on the map plane, including tilted view; they are not 3D objects.
- No saved map fields, IDs, room positions, hit testing or navigation change.
- Superzone tree packs remain supported. The three new ground/field sheets are
  shared globally and can be replaced through the existing resource directory.

## Implementation

`display/TerrainSprites.h` generates nine jittered anchor positions from a fixed
32-bit hash of the room's **external (saved) ID**. Each anchor stays within its
room; a shared bottom-centre image anchor lets foliage extend north and sideways.
This is the saved map room number, not the MUME server ID; unknown server IDs
do not affect seeding. Changing a saved room ID can change its tree layout. A rebuilt batch or restart
with the same IDs retains the layout.

`MapCanvasRoomDrawer.cpp` generates and sorts sprite instances asynchronously
alongside the existing map batches. Sorting uses descending world Y (north
first), then position/size/image as deterministic tie-breakers, across all rooms
on each elevation. Variants are never grouped into separate draw calls.

Trail clearance uses the same `getRoadIndex()` exit flags as the existing trail
artwork. Each flagged direction contributes a centre-to-edge corridor; their
union handles straight paths, bends, dead ends and junctions. Candidates inside
the corridor are skipped without moving or replacing them, so the number of
trees decreases while density and individual tree identities outside the trail
stay unchanged. The half-width is 0.16 room units, with an additional 0.035 room
units of clearance for tree bases. This clears seed points and trunks; canopies
may overhang the corridor, with the trail still drawn above them as usual.

`TerrainSpritePacks.cpp` resolves optional superzone packs from the room's saved
`Area` name. No hardcoded list of region names or new map fields is needed.
`Textures.cpp` uploads the resolved artwork into a single texture array.

Each pack has six tree identities in four seasons. They occupy two 512-pixel
square atlas layers, with sixteen 128-pixel cells per layer (eight cells unused).
Forty-four different superzones plus the shared fallback require at most 90
layers, below the OpenGL 3.3 / GLES 3.0 minimum of 256 array layers. Aliases share
a pack; missing pack directories share the fallback. Only packs named in the
manifest are loaded. Mipmaps are generated per cell before packing, preserving
alpha without mixing neighbouring tree artwork.

A 32-byte instance records anchor, size, elevation, atlas tile, tint, seasonal
timing and geometry kind/quarter-turn rotation. Each supported room has a
seasonal undercoat quad and nine ground patches. Forest adds up to nine trees
(608 bytes total before trail clearance). Fields keep roughly half of nine
foreground candidates and then apply trail clearance.

The four shared sprite families require eight 512-pixel atlas layers, about
10.67 MiB. With 44 distinct regional tree packs there are 96 layers (about
128 MiB), below the GL/GLES minimum of 256 layers. Existing wrappers retain
individual 2D source textures too, so budget roughly double those GPU totals:
21.33 / 256 MiB. CPU source images are released after upload.

Ground uses an independent saved-room hash, size variation and quarter-turn
rotation. A stable hashed painter order interleaves forest/field fringes across
room boundaries. The shader adds a broad feather to the artwork's own irregular
alpha edge. A common seasonal undercoat guarantees coverage below the fringes,
changing smoothly across the full 28-day window without room colour boundaries.

Four instanced passes draw undercoat, ground, optional tree shadows, then the
foreground sorted north to south (trees and field details together). Only the
undercoat writes ground depth. The later passes leave it intact for navigation
overlays. Grass/flowers do not cast the trees' projected shadows. No clock-driven
sorting, per-object updates or buffer uploads are needed.

The bundled `terrain-trees-seasons.png` contains six columns of tree identities
and four rows: spring, summer, autumn, winter. It was made with the built-in
image generator using the prompt recorded in `docs/seasonal-tree-artwork.txt`.
It has genuine alpha transparency. The illustrative seasonal variants aim to
preserve each tree's trunk and branch arrangement; generated drawings are not
pixel-registered animation frames and should receive visual review before
final artwork acceptance. The loader does not alter their branch geometry.

Ground, shadow and foreground passes follow undercoat/tints and precede
streams/trails/overlays/walls.
It uses standard alpha blending with depth testing disabled; OpenGL consequently
does not write depth. This allows overhangs while leaving the ground depth
available for the existing overlay passes. The selected elevation has no final
layer-fading pass; other elevations use their original artwork.

The new instanced mesh follows MMapper's existing OpenGL 3.3 / GLES 3.0 wrappers.
No new library, compute shader, geometry shader, saved randomness or per-frame
sorting is required. The existing whole-map batch invalidation is retained;
there is no new chunk cache or per-room scene-object system.

## Moving sun and moon shadows

The existing GameObserver clock tick supplies the current MUME moment. Once a
known game time has been observed, a single map-wide projection is calculated
per tick (one real second, one game minute). No new animation timer, astronomy
service or per-tree CPU update is required. Before a known time is available,
the map renders trees without directional shadows.

Sun positions use a fixed illustrative latitude of 52.5 degrees north, as a
Hobbiton/Birmingham approximation. The midpoint of MMapper's monthly dawn/dusk
table defines solar noon. An apparent solar declination is solved from that
same day length so shadows agree with the **game's** sunrise and sunset. This
is a game-aligned sky approximation, not an Earth-calendar ephemeris or a
geographic latitude calculation for every room on the map.

The latitude/declination/hour-angle projection follows standard solar geometry
(see [NOAA's solar-position equations](https://gml.noaa.gov/grad/solcalc/solareqns.PDF)).
Morning shadows point away from the eastern sun; midday shadows point north;
evening shadows move away from the western sun. Low-angle shadows are capped
at three tree-height units and fade close to the horizon, avoiding infinities
and very long map-spanning polygons.

MMapper already provides `moonZenithMinutes()`, `moonPosition()`,
`isMoonBelowHorizon()`, `moonVisibility()` and a phase-dependent light level.
At night these drive much fainter moon shadows, with strength scaled by the
square of the tracked light level. The existing twelve-hour moon sky arc is
preserved; no lunar declination or orbital model is invented. Invisible,
below-horizon and new moons cast no shadow. Moon shadows fade during the half
game-hour adjacent to sunrise/sunset; daylight uses the sun alone.

The tree batch and alpha images are reused for one extra instanced shadow pass
before the normal tree pass. Only one uniform vector changes as time advances;
seed positions, sorting and GPU instance buffers remain untouched. The shader
projects silhouettes away from their anchored trunks on the map plane. Permanent
dark/shaded rooms have no direct-light shadow. Existing trail, symbol, wall and
character overlays remain above both passes.

This is inexpensive projected sprite artwork, rather than 3D shadow mapping.
It does not relight highlights baked into the tree images, simulate canopy
self-shadowing, derive different latitudes across Arda or compute cloud/building
occlusion. Shadow appearance can be refined without changing the seed layout.

## Superzone packs and seasonal transitions

**Preferences → Graphics → Room Details → Seasonal terrain** is enabled by default
when using organic terrain. Turning it off selects summer ground, tussocks and tree artwork (including
any regional all-year override). Unknown game time also selects summer until
MMapper observes time. No real-world date is used.

Season boundaries follow `MumeMoment::toSeason()`: day 0 winter, day 90 spring,
day 180 summer, day 270 autumn in the 360-day game year. Each transition spans
14 **MUME days** before and after the boundary, including the winter year wrap.
Each tree has its own fixed hash-derived change time spread over 24 days and
a four-day crossfade, for a total 28-day window. Trees keep their saved-room
seed, variant, size and position. A map rebuild, restart or pan does not shuffle
their transition order. Colour and leaf coverage change while the tree identity
stays paired; premultiplied colour interpolation avoids dark fading edges.

Add `pixmaps/organic-terrain/packs.json` under the existing custom resources
directory (or sideloaded assets directory). Names on the left are the exact
saved room area/superzone strings, including case. Names on the right are simple
folder names containing letters, digits, `_` or `-`:

```json
{
  "superzones": {
    "Example deciduous area": "deciduous",
    "Example evergreen area": "evergreen",
    "Another area sharing trees": "deciduous"
  }
}
```

These are illustrative names, not assumed game names. Supply at most 44 entries;
rooms with missing or unrecognised areas use the shared fallback. There is no
need to populate all 44 superzones. Restart after editing the pack files.

Place `spring.png`, `summer.png`, `autumn.png`, `winter.png`, and optionally
`all.png` inside each named folder. Each is a transparent **three-column,
two-row** sheet of six equal square cells, in the same identity order. For different regional tree silhouettes, supply a matched full seasonal family
or an `all.png` base so missing seasons do not borrow an unrelated default
tree. Keep the
same canvas, skeleton, scale and trunk anchor (centre X, 94% down from the top)
in every seasonal sheet. `all.png` is an explicit all-year choice, useful for
conifers or unusual regional trees. The `default/` folder optionally overrides
the shared seasonal fallback. An empty manifest enables default-only overrides.

For each requested season, resolution is:

1. Superzone's sheet for that season.
2. Superzone's `all.png`.
3. `default/` sheet for that season.
4. `default/all.png`.
5. Bundled sheet for that season.

Missing/unreadable/non-square-cell sheets follow the same chain; invalid JSON
uses the default pack. Invalid folder identifiers are ignored. All choices use
local resources only. This patch ships one shared deciduous set and support for
regional sets; it does not claim to include 44 distinct art sets. Other terrain
sectors retain the existing artwork in this experiment.

## Forest/field sheets

Three additional bundled transparent PNGs each have six columns of variants and
four rows (spring, summer, autumn, winter):

- `terrain-forest-ground-seasons.png`: flat moss/humus, autumn leaves, then snow
  with a few twig and leaf fragments.
- `terrain-field-ground-seasons.png`: short green grass, straw/olive autumn grass,
  then soft snow cover.
- `terrain-field-details-seasons.png`: small spring flowers, longer summer grass
  tussocks, straw autumn tussocks, then sparse stalks with snowy bases.

Ground images are centred within their cells; their ragged alpha fringes mingle
across room/sector boundaries. Ground, trees and field details use independent
seed streams but the same ±14-MUME-day seasonal window and individual four-day
fades. Foreground seeds respect the trail corridors without refilling; ground
continues beneath trails. Turning seasonal terrain off or having unknown game
time selects summer.

Override any full sheet by putting its filename in `pixmaps/` under the existing
custom resources/sideloaded assets directory. Missing/unreadable/incorrectly
sized overrides use the bundled sheet. Restart after changing artwork. These
are prototype assets, rather than a pixel-for-pixel reproduction of the earlier
concept image. Artwork prompts are recorded in `docs/seasonal-ground-artwork.txt`.

## Applying this patch

Base: upstream MUME/MMapper master commit `6a0ae0e`.
Version 5 replaces versions 1, 2, 3 and 4 completely; it is not incremental.
If an earlier version has already been applied, reverse that version before
applying version 5. Do not stack the patches. The patch includes binary
PNG data: use `git apply`, rather than copying the patch text into a source file.
For a branch with other changes, check applicability first:

```sh
git apply --check ~/Downloads/mmapper-organic-forest-v5.patch
git apply ~/Downloads/mmapper-organic-forest-v5.patch
git diff --check
```

Then rebuild your existing build directory and enable the preference above.
This patch was prepared against upstream master; compatibility with a separate
character-token branch must be established by the check, rather than assumed.
If the check reports a conflict, do not use `--reject` or force it through.

To remove only this patch, provided none of its affected lines have since been
edited:

```sh
git apply --reverse --check ~/Downloads/mmapper-organic-forest-v5.patch
git apply --reverse ~/Downloads/mmapper-organic-forest-v5.patch
```

## Validation

- Full Linux Debug build with GCC 13.3, Qt 6.4.2, ccache, AddressSanitizer and
  UndefinedBehaviorSanitizer. Audio/default-map/external-image downloads were
  disabled for validation; all four seasonal sheets are bundled Qt resources.
- Both terrain CTests passed, including deterministic saved-ID layouts, trail
  masks, ground/field bounds, independent seed streams, seasonal atlas layout,
  fallback resolution, aliases, malformed inputs, and sun/moon projection limits.
- The actual GLSL shaders and final art were rendered on desktop OpenGL and
  OpenGL ES. Winter, spring, summer, autumn, the autumn transition and unknown
  game time produced matching desktop/ES frames. A wall at the original ground
  depth stayed visible above undercoat, ground patches and foreground sprites.
- Clock-fed MMapper application checks covered winter snow, spring flowers,
  summer tussocks, autumn, seasonal variation disabled, and organic mode disabled.
- Clean application to the stated base, byte comparison of changed files and
  reverse-application check passed.

The CTests used LeakSanitizer disabled because this container cannot perform its
process inspection. Native macOS, Windows and WebAssembly builds were not run.

For local visual acceptance, inspect dense forest, isolated forest rooms, a
forest/field boundary, dark forest, paths and special walls. Pan, change zoom,
switch elevation, toggle the preference, and restart. Check the tree positions
remain stable and the navigation overlays remain readable. Performance on a full
player map and native macOS/Windows/browser builds needs local verification.

Artwork density, proportions and the zoom cutoff are intentionally fixed for
this first experiment. Further terrain types can reuse the same layout and draw
path after visual review of forest and field.
