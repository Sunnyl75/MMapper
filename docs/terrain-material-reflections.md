# Material-specific terrain reflections

Incremental follow-up to `mmapper-specularity-preview-v1.patch`. The previous
preview, water/road/flow and indoor-track patches must already be applied.
Existing mode/brightness settings (including 150%) and preview-clock behaviour
are preserved. Preferences now labels the setting **Terrain reflections**.

| Material | Selection | Surface response |
| --- | --- | --- |
| Water | Existing painted highlight mask | Original generated normals and moving reflection band |
| Shallows / marsh | Muted pools; exclude saturated reeds/mud and bright white marks/snow | Pool-wide reflectivity with static fine normal texture |
| Rapids | Wet blue/grey water and softer white foam; exclude saturated earth/vegetation colours | Painted flow normals; foam stays softer than ordinary water |
| Mountain peaks | Bright, low-chroma white/pale caps | Painted normals follow the existing upright mountain-facing proxy |

Shallows water can be olive in summer and tea coloured in autumn, so its mask
uses muted colour/saturation as well as brightness rather than blue alone.
The additional fine relief is deterministic and confined to selected wet
pixels. Colour artwork is unchanged; it is not an extra image, animated noise,
or newly painted ripples. The separate upright reed sprites are not reflective.

Mountain caps use their own facing-based response rather than a ground-water
stripe. Backlit fronts do not glint through the existing self shading. Shadow
passes are unchanged and direct sky reflections remain disabled by the existing
indoor, underground, underwater and permanent-shade flags. The same live/preview
sun and visible-moon source and existing weather overlays remain in use.

All data comes from the final, aligned GL-oriented colour atlases, so UVs,
quarter-turn rotations, water flips, seasonal blends and fixed rapids variants
match the visible artwork. The marsh underpaint and its ground sprites both
sample their material data. Reflections do not change sprite alpha/coverage.

`TerrainMaterialMaps.h` declares one four-entry source/profile table. A single
low-priority worker generates or reads four two-page data packs, then the next
normal GL frame uploads the eight pages together. There are no new drawing
passes, map scans or mesh rebuilds. Relative to the first preview this adds six
512px RGBA texture-array layers (about 8 MiB including their cell-local mipmaps).
Cache keys include artwork, algorithm version and surface profile. The existing
application-cache folder retains at most 32 PNG files for these profiles.

These automatic masks are colour-based estimates, not semantic recognition.
Custom artwork with different colours may need material presets or an optional
authored mask in the future. A separate sprite editor/runtime dependency is not
introduced. The generated selections and actual shader output are included in
the review bundle for checking these defaults against the bundled artwork.
