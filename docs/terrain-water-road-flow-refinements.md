# Water coverage, road repetition and paired current artwork

Incremental patch for terrain checkpoint `c209354778e70b4492e09aeb079f331021ea5cb8`.
The same patch applies to combined terrain/token checkpoint
`39fa30436e60b8ba18c88916c3ab1a53803652dc`.

## Changes

- Water and rapids keep their complete authored frames. The old alpha-64
  alignment crop discarded faint paint and could produce rectangular cuts.
  A tiny fade at the sprite quad edge protects against an imperfect source
  frame; wall and mapped-water clipping still occur at the actual boundaries.
- The six water patches occupy six jittered regions, covering both ends of
  a room without increasing the sprite count or changing tree/field scatter.
- Water underpaint samples the same seasonal water artwork, using the opaque
  centre of the broad oval variant. Exposed underpaint has water detail and
  the matching palette instead of a flat, differently coloured grey fill.
- Road surfaces and road shore material blend independently selected,
  quarter-turned authored variants at shared world-space nodes. They no longer
  mirror a single gravel image into a repeated diamond motif. Room borders
  do not restart the material coordinates.
- **Trails keep their existing texture sampling, colours and seasonal rules.**
  In particular, underground trails retain their existing fixed material.
- Current indicators use one northbound painted wave ribbon. An outgoing
  stream samples its first half; the incoming counterpart samples its second
  half. Each quad ends at its owning room boundary. Existing stream traversal,
  incoming/outgoing detection, flags and up/down fallback remain unchanged.
- Dawn/dusk and token behaviour are unchanged.

## Artwork

`src/resources/pixmaps/terrain-water-flow-ribbon.png` is the new all-season,
true-alpha source. It is a single square image; the loader expands it to the
existing fixed-role atlas layout. Halves are sampled in the shader, not
generated separately, so their shared edge is identical by construction.
No extra atlas layers are needed. The previous `terrain-water-flow.png`
remains in the source tree for reference but is superseded for rendering.

Created using the built-in image-generation tool. Final prompt specification:
one square transparent sprite with five closely spaced north-pointing, softly
rounded wavefronts; muted cyan/blue-grey washes, pale foam ridges and comic
ink strokes; a continuous centre that can be split horizontally into matching
halves; organic transparent outer edges, no background panel, shoreline,
geometric arrow icons or winding river silhouette. Style references were the
supplied stream image and existing bundled water artwork.

## Validation

- Linux GCC C++20 checks through ccache, with `-g -O0`, `-Werror`, `-Wshadow`,
  `-Wfloat-equal`: `TestTerrainEdges`, `TestTerrainSprites` and Qt 6.4.2
  `TestTerrainSpritePacks` passed.
- Modified production `MapCanvasRoomDrawer.cpp` and `Textures.cpp` passed
  C++ syntax checks against the available Linux Qt 6 headers.
- Production shaders compiled and passed 2,169 render assertions each on
  Mesa desktop OpenGL 4.5 and OpenGL ES 3.2. These include an outgoing/incoming
  pair identical to the uncut ribbon, real-art road continuity across room
  boundaries, all-season road asymmetry, textured opaque water fill, existing
  wall clipping, door rendering and fixed underground trail behaviour.
- Modified C++ regions formatted with clang-format 18; `git diff --check` passed.

A complete Mac app build and Mac GPU timings were not run here. Material
blending uses up to four texture samples per surface (twice that during a
season blend), without adding redraw requests, instances, network queries
or movement callbacks. Review appearance and GPU usage in the existing
Qt Creator project before committing.
