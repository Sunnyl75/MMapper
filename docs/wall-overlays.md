> Historical stage documentation. Installation/base and preview-clock statements below describe that stage. For the reconciled checkpoint version, use [terrain-checkpoint-reconciliation.md](terrain-checkpoint-reconciliation.md); do not apply the historical patch commands below.

> This describes the initial tunnel/cavern wall update. The subsequent
> [city and indoor extension](city-indoor-terrain.md) adds timber, shingles,
> gutters, grime and wall shadows; its maximum atlas allocation is 136 layers.
> The two-family figures below describe the earlier underground update.

# Underground wall overlays and indoor shadow exclusion

This update adds a separate batched wall-sprite pass above MMapper's existing
wall rendering. It follows the existing solid-wall visitor callbacks, rather
than reconstructing another wall topology. Open connections retain their gaps.

Tunnel rooms use buff rubble wall art; cavern rooms use cooler fractured
limestone. Each room draws its own inward-facing half-border, entirely within
its unit square. Shared edges can therefore have different materials on their
two sides. Perpendicular strips overlap inside their owning room to join corners.
The importer positions each strip against its cell boundary and extrudes its
opaque edge texels, covering the underlying line without an outward overhang.
Twenty-four stable variants per material are seeded from the saved room ID and
direction. Underground walls are all-season and use the normal room darkness tint.

There is deliberately no generic wall-art fallback. Other environments and doors
retain MMapper's normal rendering. Invalid/missing bundled wall art reserves
transparent atlas pages and disables the affected overlay family. A resource
override can fall back to the matching bundled tunnel/cavern material, but never
to another environment's material. No new wall/door connections are created.

The overlay follows the Organic terrain setting, selected elevation and existing
zoom threshold. It adds one instanced batch, not a draw call per room or wall.
The two new sheets take four atlas layers, bringing the worst-case total for
44 regional tree packs plus fallback and shared materials to 128 layers.
The existing 11-bit shared tile encoding still fits (last used tile 2039).

## Indoor rooms and celestial shadows

MMapper classifies rooms through the existing terrain enum: INDOORS, TUNNEL and
CAVERN. There is no separate persisted indoor boolean in this checkout. The
shared helper uses those three values to reject sky shadows, irrespective of
whether the room is lit. Underwater, dark and NO_SUNDEATH rooms are also excluded.
A static per-instance flag suppresses the solar/lunar projection pass, including
future indoor tree packs. No room flag, map serialization or preview clock
format is changed; ordinary indoor shading remains in effect.

## Validation

Linux Debug with ccache: MapCanvasRoomDrawer.cpp, Textures.cpp and MapBatches.cpp
compile. TestTerrainSprites, TestTerrainEdges, TestTerrainSpritePacks and the new
TestTerrainWalls pass. The latter checks indoor shadow eligibility, absence of
wall families for unsupported environments, all cardinal rotations, capacity,
and the actual imported artwork's opaque boundary/transparent interior.

Production GLSL render tests pass 1420 assertions on each of Mesa desktop
OpenGL 4.5 and OpenGL ES 3.2, including wall confinement, missing-art transparency,
explicit indoor shadow suppression, and all 24 real wall variants in all four
directions with the runtime mipmaps at reduced map scale.

```sh
ctest --test-dir build --output-on-failure -R 'TestTerrain(Sprites|Edges|SpritePacks|Walls)$'
./build/tests/TestTerrainWalls /tmp/mmapper-wall-atlases
python3 tests/terrain_edges_render.py gl /tmp/walls-gl.png /tmp/mmapper-wall-atlases
python3 tests/terrain_edges_render.py es /tmp/walls-es.png /tmp/mmapper-wall-atlases
```

The generated wall-atlas diagnostics use the production shaders and imported
assets; they are not app screenshots. A full Linux Debug application link/run has now also been completed. The
app capture uses the real MainWindow and production renderer with a synthetic
12x8 tunnel/cavern map: no trails, dividing walls with openings, and lit/dark
rooms. Native macOS validation has not been performed.

## Full-app integration fixes

Optional organic meshes are checked before rendering. In particular, a map
with no roads/trails has no terrain-edge mesh; trying to render that empty mesh
previously raised NullPointerException and stopped the frame before the wall
pass. The same guard covers absent ground, foreground and wall meshes, so
unsupported wall environments remain on MMapper's normal rendering path.

Native builds use Qt's `qt_add_big_resources` for the large atlas bundle. This
embeds the same resource data without parsing a huge generated C++ byte array
and exhausting compiler memory. WebAssembly retains its existing resource path;
Qt handles the documented iOS/Xcode fallback inside its large-resource helper.

## Applying

`mmapper-organic-terrain-v6.patch` is the updated complete replacement patch
against upstream `6a0ae0ec08d3add95c0f0eb425ed04be9f807799`. Do not stack it on
an earlier organic-terrain patch.

`mmapper-wall-overlays-v1.patch` is an incremental alternative for the exact V6
version previously delivered with foreground fitting and background clipping
at walls (Library version 5). Inspect and check before applying:

```sh
git apply --stat /path/to/mmapper-wall-overlays-v1.patch
git apply --check /path/to/mmapper-wall-overlays-v1.patch
git apply /path/to/mmapper-wall-overlays-v1.patch
```

The unavailable local Terrain Preview Clock work still needs merging with the
terrain changes. Preserve that branch/stash; do not overwrite it to force a patch.

## Artwork

Bundled files: terrain-tunnel-walls.png and terrain-cavern-walls.png. Generated
with the built-in image tool using the approved room-constrained wall mockup as
style reference. Prompt: 6x4 atlas, 24 all-season top-edge-only strips, narrow
inward-facing low-relief comic stone, warm buff rubble for tunnels/cool fractured
limestone for caverns, opaque boundary, jagged interior silhouette, transparent
lower cell, no floor or scene, no outward overhang, no text or seasonal snow.
Source sheets remain unchanged; CPU import crops row gutters, fits the strips
into the top fifth of a cell and extrudes boundary texels before normal packing.
