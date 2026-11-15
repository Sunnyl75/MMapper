# Shire artwork refresh and shared road overlay

Roads use normal earth underpaint and surrounding terrain fringes, with optional
world-seeded centre grass sprites shared by all packs. See
`terrain-optional-pack-details.md`. Both former painted rut systems are removed.

## Artwork changes

- Richer brown dirt includes fine stones and very sparse grass. The previous
  winter road artwork is retained. Separate broad low clover/grass tufts match
  the revised field palette and fade out under winter snow.
- Field ground and detail leaves use softer moss/fern greens, retaining their
  existing seasonal rows, flower colours and silhouette layout.
- Indoor boards run horizontally. The source is packed at a smaller scale;
  the existing repeating-material loader keeps world-space continuity across
  rooms and repairs repeat boundaries at every mip level.
- Closed doors have a shallow overhead curved leaf rather than a face-on disk.
  Open doors retain only their frame/posts. The existing shared door loader
  removes the leaf, using the same anchor and frame in both states. No swung
  door leaf is displayed in the open state.

## Image-generation prompt set

The built-in image generator produced the replacement artwork. Atlas packing
and resizing preserve the renderer's existing formats.

1. Field ground: edit the exact 6x4 seasonal atlas, preserve every footprint and
   alpha, lower lime/yellow saturation and brightness, use softer moss/fern
   greens, retain white flowers and winter snow.
2. Field details: edit the exact 6x4 tuft atlas to match the new ground palette,
   preserving shapes, anchors, seasonal rows, seed heads, flowers and snow.
3. Indoor floor: seamless strictly overhead honey-oak texture with 16 narrow
   horizontal staggered board courses, fine grain and seams, no walls, border,
   perspective or edge highlights. Four copies are packed into the repeat
   source to give approximately eight courses per map room.
4. Road background: square opaque overhead warm medium-brown compacted earth,
   fine embedded pebbles, subtle mottling, under 3% tiny green flecks; no tracks,
   road edges or strong directional features. Six crops are packed into the
   existing seasonal road atlas for spring/summer/autumn.
   Road tufts: transparent 3x2 square-cell atlas of six low irregular short-grass
   and clover patches, 30% broader than the approved concept, matching the
   spring/summer field palette. Porous gaps, compact oval footprints, no tall
   bushes, flowers, ground or shadows. Built-in image generation used references
   to the approved road board and revised field-ground atlas.
5. Doors: preserve the normal overhead atlas's six-colour 6x2 layout and posts;
   use very shallow foreshortened rounded hobbit-door leaves. Leave transparent
   clearance between the post caps above the leaf so the existing loader can
   find the opening. Open row contains only matching posts/jambs, no leaf.

## Validation and remaining acceptance

The production OpenGL and GLES shader tests cover real Shire art, all road
connections, walls, absent foreground, compatible adjacent world-space joins,
room-seed independence and grass confinement. Software rendering does not
measure Mac GPU performance.

The complete Qt application has not been built in this environment: Qt
development dependencies and ccache are unavailable. Rebuild the usual local
project and inspect the Shire at several zooms before committing. The existing
locally committed darkness and clock changes must be retained.
