# Preliminary reflections and preview playback

The material extension described in `terrain-material-reflections.md` adds
shallows, rapids and mountain caps. The original water-preview notes below
record the first version; the extension supersedes its water-only scope,
two-page total and eight-file cache limit. The clock controls are unchanged.

This incremental change targets the terrain-only branch at `c2093547`, after
the water/road/flow and indoor-track patches. It does not include tokens or
replace those earlier patches.

In Preferences → Graphics, Water reflections offers Off, Reflectivity mask,
and Mask and normal map. Reflection brightness ranges from 0–200%. Defaults
are mask mode and 100%. The mode and brightness are saved normally. Organic
terrain must be enabled and the map must be above its existing detail cutoff.

Water source colour images are unchanged. `TerrainMaterialMaps.cpp` generates
reflectivity from local painted contrast and broader light-blue ridges, and
shallow normal vectors from filtered luminance slopes. Alpha borders are
excluded. Normal RGB and reflectivity alpha are raw linear data, never
premultiplied colour. Mipmaps are made within cells and renormalize vectors.

Two extra 512px atlas layers contain all 24 seasonal frames. There are no new
draw calls or room-instance buffers. The water underpaint and overlapping
patches sample matching material data, including season blends and UV flips
and quarter turns. Rapids retain their existing foam overlay. Marsh and other
sectors are outside this first build.

Preparation runs on one low-priority CPU worker after neutral placeholders are
reserved. Its short polling timer stops on completion. A normal GL frame then
uploads the data without rebuilding the map. Results are cached beneath Qt's
application cache location in `terrain-materials-v1`, keyed by source pixels
and generator version. Missing/corrupt cache files regenerate, unwritable cache
directories still allow in-memory use, and old entries are bounded to eight
PNG files. This cache is shared across game maps using the same artwork.

Reflections use an unclamped sky direction and sky colour from the same live
or preview MUME moment as the weather. Native dawn/dusk warning hours anchor
the timing. Daylight and faint visible-moon modes are supported. Existing
indoor/underground/underwater/dark/permanent-shade flags suppress sky reflections.
The current night and twilight overlays retain their existing passes.

This is a stylised direction cue rather than a complete physically based
water renderer. It adds no animated waves, ripples or noise. Broad room-centred
envelopes blend through neighbouring room centres to avoid a hard lighting cut
at room boundaries. Normal mode changes local surface response; flat normals
are normalized to match mask-only brightness.

The Terrain Preview Clock remains under View → Terrain Preview Clock. Enable
Use preview time, then use the horizontal Playback slider:

- Centre: 1× forward, one game minute per real second.
- Immediately left: 1× reverse; farther left accelerates reverse playback.
- Right: forward playback faster than regular time.
- Both ends: 1024×; pause is a separate checkbox.
- Normal speed recentres the slider and unpauses.

No slow-motion forward range is used. Elapsed real time drives a fractional
accumulator, so low speeds do not drift with timer jitter. Calendar carry and
borrow apply in both directions, with the existing year limits. The timer is
inactive when preview is disabled or paused. The live MMapper clock, game
parser and GMCP remain independent. High playback speeds deliberately generate
more preview redraws; they are a testing facility, not a gameplay animation.

The appearance-control todo list is `docs/terrain-roadmap.md`. Other requested
grime, atmosphere and night sliders are future work, not part of this patch.
