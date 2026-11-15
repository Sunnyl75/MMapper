# Optional connected terrain details

Named/manifest packs may include these optional PNGs:

- `terrain-road-centre-grass.png`: six low transparent grass tufts on a 3x2
  square-cell grid. Their silhouette and blade contrast supply the foreground;
  rendered hue comes from the pack's actual seasonal FIELD underpaint.
- `terrain-city-wall-fringe.png`: six horizontal wall-foot strips on a 3x2 grid,
  with alpha trailing away from the wall. Top of a strip is the wall side.

These use the normal ordered resource roots and texture layer limits. Invalid
or absent files have no effect. The terrain family enumeration is unchanged;
metadata columns 200/201 hold availability and tile bases.

## Shared road foreground

Roads retain their existing ROAD earth underpaint, normal geometry and terrain
fringes. Optional tufts are composited into that same road draw, confined to a
narrow centre corridor selected by the existing NESW connections, excluding
walls. Bends follow quarter circles with perpendicular endpoint tangents;
straight arms, dead ends and junctions share the same centre coordinates.

Tufts form one sequence along the centreline, with slight independent lateral
and longitudinal offsets. A straight or bend has eleven interior tufts plus a
shared half-tuft at each end: approximately twelve per room. Junctions continue
that spacing along each connected arm. There is no two-dimensional placement
grid and no parallel row of tufts to either side of the centre.

Each connected room edge owns one shared world-seeded tuft, sampled identically
by both rooms. Interior sprites fade before that edge, avoiding clipped halves
or mismatched stamps when a straight joins a curve. Rotation and variation use
fixed map anchors, independent of room seeds, camera position and zoom.

All packs may opt in using this filename; no region name is tested by the
renderer. Existing trails and packs without tufts retain their behaviour.
The static green tufts fade out with the existing winter factor; normal seasonal
road artwork supplies snow. The Shire retains its previous winter road row.

The former `terrain-road-cart-tracks.png` and experimental
`terrain-road-overlay.png` systems are removed. These are not input aliases.

## Other details and validation

Walls retain their explicit tile in the low 20 bits; high bits carry the pack
selection. Optional fringe replaces the narrow procedural gutter within a
0.065-room band, respecting existing rotations, clips and end/corner geometry.
Packs lacking fringe keep their original 0.012-room gutter.

No map scan, timer, lighting-index update or extra draw pass is introduced.
Candidate sprites outside the fragment are skipped; only overlapping tufts
are sampled. Desktop GPU performance still needs local measurement.

`tests/pack_details_render.py` exercises production GL/GLES shaders, all 15
connection masks, every compatible adjacent boundary at actual world positions,
room-seed independence, centre-band confinement, walls and missing art. It can
use real Shire artwork as its third argument. Software rendering does not
replace visual acceptance or performance testing on macOS.

## Full grass texture and worn lanes

The foreground uses the actual seasonal FIELD artwork via `groundFill`, rather
than reducing it to one core colour and multiplying it by the old tuft shading.
The twelve staggered anchors remain, but each sprite footprint spans 0.56 rooms.
Their overlap forms a broad dense median with ragged edges. Connected centreline
distance also grows grassy shoulders, exposing two wheel-worn dirt lanes between
the median and verges. No separate painted rut geometry or regional branch is
added. World-space edge variation stays continuous across shared room boundaries.
Normal road earth, connected masks, walls and draw order remain in use. Winter
still fades this foreground and retains the existing snow-road artwork.

Tests require broad median coverage, grassy shoulders and two bare dirt lanes,
in both orientations, as well as palette changes following FIELD texture changes.

## Lush Shire hills

The Shire adds `terrain-hills-seasons.png`: six new rounded hill silhouettes in
spring, summer, autumn and winter rows, using the existing UPRIGHT material slot.
Grass and clover match the field reference, rock exposures are smaller, and
soft grassy feet replace dry ochre fringes. `terrain-hill-ground-seasons.png`
uses an exact copy of the Shire FIELD ground atlas, matching the surrounding
floor beneath these hill sprites. Other regions retain their current hill art.

The hill atlas was made with the built-in image generator. Prompt: retain the
existing six-column/four-row seasonal layout and elevated map viewpoint, create
six varied rounded knolls/ridges/saddles, coat them in the reference field's
lush moss/fern grass and clover, soften occasional grey outcrops and grassy
feet, remove heavy black outlines and dry yellow turf, retain snowy winter,
use real transparent margins with no backdrop or cast shadow. The reference
images were the default hill atlas and the revised Shire field-ground atlas.

## City joins and fixed hill definition

Open ROAD-to-CITY boundaries now contribute a route connection even when that
exit lacks a separate ROAD flag. CITY already supplies its street paving; the
road therefore reaches that boundary rather than drawing a rounded dead end in
its own room. Doors/no-exit walls still block the connection. Existing ROAD and
non-city peer rules are retained. This applies to every organic terrain pack.

The Shire hill atlas retains its lush forms and seasonal layout, with fixed
local shading added to recesses, saddles and lower slopes. This is baked into
the sprite rather than tied to the live sun direction. No external cast-shadow
ellipse or heavy outline is added. The image edit used the existing atlas as
its reference: preserve six forms, plants, colours, seasons and transparent
footprints; add restrained ambient occlusion in valleys and at the grassy foot,
with a subtle crest highlight. Mechanical packing restores common grid anchors.

## Soft city/road joins and clean trail aprons

CITY paving carries the pack row and open ROAD-neighbour mask without changing
its explicit texture index. The vertex shader removes the pack bits before
sampling the paving atlas, as it already does for walls. INDOORS uses its
existing paving path and receives no city grading or join blend.

Open city/road joins crossfade toward one shared world-space material mixture
within 0.18 rooms on each side. The shared earth sample uses the summer road
artwork's opaque core, avoiding undefined RGB in transparent atlas gutters and
keeping static paving/seasonal road samples consistent at their boundary.
Packs with optional road grass (including the Shire) mix 15% of this brown earth
into the city surface. Walls and doors still block transition masks.

A route apron now draws only its road-earth surface coverage. It no longer
paints the full road's decorative border/verge outside that coverage, which
could leave thin curved roots/grass tails beside a trail junction. Normal
trails and ordinary road verges retain their own artwork.

## Optional wall gardens

`terrain-city-wall-gardens.png` contains six individual garden huddles on a
3x2 square-cell atlas, with real transparent margins and irregular flowers and
foliage. Unlike `terrain-city-wall-fringe.png`, cells are not vertically cropped
or stretched into a strip. Lookup column 202 records availability and tile base.
Garden art takes priority over the older optional fringe (201); packs without
gardens retain their previous fringe/channel behaviour.

The Shire replaces its continuous green fringe with this garden atlas. Actual
square sprites, rather than stretched wall-strip samples, now place the images.
The placement and rendering contract is described below. Other packs retain
their existing optional fringe behaviour when no garden atlas is supplied.

The built-in image generator made six low overhead cottage-garden clumps using
the Shire field atlas as the greenery reference: white daisies, yellow flowers,
rose-pink flowers, lavender-purple spikes and two mixed clumps; irregular leafy
footprints, genuine alpha, no soil rectangle/hedge/pots/wall. A second image edit
increased the flower heads about 70%, simplified their petals, and strengthened
light/colour contrast for readability at small map sizes, while retaining the
six plant compositions and transparent margins.

## Proportion-preserving garden placement

`TerrainGardens.h` creates twelve full square sprites per solid city wall from
the saved room ID and wall direction. Sizes vary uniformly between 0.18 and
0.22 rooms, with small independent offsets along and away from the wall. Close
spacing deliberately overlaps the clumps. Full quads stay inside their room;
clearances beside perpendicular walls and door approaches prevent abrupt corner
cutoffs and leave the street centre clear. Quarter-turns change orientation,
never aspect ratio. Rebuilding the same room produces identical placements.

A cached instanced garden batch is painted after walls/roof corners and before
doors. Garden images use the existing generic overhead sprite path with a
rotation flag, not wall UVs, gutters, end fades or corner discard rules. This
adds one draw call per garden-bearing layer; placement runs only during mesh
construction. The old garden gutter sampling is removed and its wall-strip
width is zero. The v9 flower artwork and its six variants are unchanged.

Pure C++ checks exercise twelve overlapping, repeatable, fully contained quads
for 400 room/direction cases. GL/GLES checks rotate every garden variant and
compare its full silhouette, then verify corner placements retain their alpha
coverage even with all wall-mask bits set. Full Qt/Mac build acceptance remains
necessary, particularly around doorways and building corners.
