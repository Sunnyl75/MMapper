# Rapids and directional flow ripples

Incremental update based on terrain checkpoint 19b07c2e, including the rounded
shoreline sprites and corrected portable atlas-limit test. It can be tested on
the terrain-only branch before updating the combined token build again.

## Rapids

Use MMapper's existing RAPIDS room sector. Its base stays identical to ordinary
water. Six independently seeded whitewater/rock/foam patches per rapid room sit
over ordinary water patches, before banks and adjoining natural ground fringes.
Their soft transparent silhouettes can overlap mapped water or rapids neighbours,
including diagonals, using the existing wall-aware masks. Land, unknown rooms
and blocking walls stop them. No new room data or server information is needed.

All 24 foam variants are fixed, while the underlying water follows the existing
seasonal palette. Saved map room numbers seed placement and variants. The
original legacy `terrain-rapids.png` remains unchanged; the new atlas is named
`terrain-rapids-patches.png` to avoid breaking non-organic terrain display.

## Flow

Use the existing stream visitor and its incoming/outgoing side observations;
there is no new connectivity inference, GMCP parsing or runtime map scanning.
Each NESW marker is a group of curved foam crests with a leading tip and trailing
wake. Incoming points into the owning room; outgoing points toward its edge.
The north-facing source image rotates as a whole, with a 180-degree difference
between incoming and outgoing at the same side. Placement remains within the
room near the relevant edge; the original flow sides are preserved.

The fixed 24-variant sheet is selected using saved map room number and side.
The bright foam is retained while the blue-grey wash between crests becomes
transparent. Existing room tint and artificial illumination apply, but no sun
shadow is cast by these flat water markings. Flow sprites remain under trails,
doors, walls and player/group markers, following the existing stream draw stage.

Native stream buffers remain available for non-organic display and other
layers. Up/down streams and missing flow artwork retain native arrows even in
organic mode. Repeated reports of the same room/side/in-out observation are
deduplicated in the new sprite batch so they do not brighten the glyph twice.

## Runtime cost and validation

All instances remain static until normal map mesh rebuilding. No animation,
additional redraw timer, gameplay callback or lighting scan is introduced.
Rapids add six patch instances per RAPIDS room in the existing ground batch;
flow uses one extra shared batch per displayed layer when markers are present.
Two atlas families add four texture-array pages (154 total with the maximum
regional tree packs, still below the existing 256-layer portability limit).
Mac GPU cost remains unmeasured.

Linux Debug/ccache tests cover flow rotations and deterministic variants, sprite
loading and the wall/atlas limit. Modified drawer and texture-loader production
files pass C++ syntax checks; existing upstream GCC warning categories are
relaxed for those checks. Actual OpenGL and OpenGL ES rendering tests cover all
24 authored ripple variants, every rotation, transparent fixed-season rapids,
wall/neighbour clipping and the existing terrain regressions. The installer is
checked for dry-run, repeat, reverse, unrelated edits, partial installs and source
mismatches. No full app link or macOS build/run was performed here.
