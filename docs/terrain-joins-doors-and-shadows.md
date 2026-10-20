# Terrain joins, foreground fitting, shadows, clock controls and doors

This increment is based on the previous underwater/brush/wall refinement package, reconstructed tree `051bd33450eb73fc9484c7830e10d362dae1fce0`. It is not a replacement for the checkpoint or the earlier cumulative terrain patch. Apply it once over that currently working version. No saved-map format changes.

## Changes

- Incoming trails connect to road surfaces using a short apron above overlapping ground. Ground overlapping roads receives a narrow irregular world-aligned fringe.
- Mountains and hills first attempt their original placement, then deterministically shrink to the largest fitting size down to 0.45 room units. Their identity and seasonal transition remain stable.
- Mountain, hill and brush silhouettes use the existing diffuse projected-shadow pass, with relative height factors 1.0, 0.55 and 0.35. Existing indoor/no-sky exclusions still apply.
- Preview month, day, hour and minute spin boxes wrap both directions. The existing 1–24 hour display, preview clock state and automatic progression remain in place.
- A fixed death-trap skull overlay is added for the existing DEATHTRAP room load flag. It does not introduce a new terrain sector or infer hazards. Original icons remain available.
- Six existing door designs, closed and open, are imported into a canonical shared frame. The door sprite pass is above existing canvas walls, doors and decorative caps. Both room-owned halves sample the same frame; paired doors share a variant seeded from saved map room IDs and axis. A one-way door has only its own half. Up/down doors keep their existing display.
- Opening is driven by explicit observed Room.Info / Room.Update.Exits state. State is transient, clears on movement/reconnect, and is never written into the saved map. An unknown remote door uses a dimmed default door symbol; this is not a claim that it is known closed. A paired door may use the observed state of its other half. Sending an open command alone is not treated as success.
- Missing door artwork and disabled organic rendering retain legacy door lines; legacy lines also remain for other map elevations. Existing underwater artwork, city trail suppression, saved-ID scatter and seasonal/regional packs are retained.

## Implementation boundaries

Door observation/variant selection lives in TerrainDoors.cpp/.h. Atlas import remains in TerrainSpritePacks, placement in the existing room-batch builder, projection/material sampling in the existing terrain shader. No independent random scatter engine or persisted door-state subsystem was added. Material texture-array capacity is now 148 layers.

New death-trap artwork: terrain-death-trap.png, generated for this change: an ivory skull/crossbones over a weathered scarlet/charcoal mark, transparent irregular edge, readable at map scale, consistent with the illustrated terrain style. Door artwork is the six-style sheet already supplied by the previous package; it is now actually used by the renderer.

## Validation and limits

Six focused Debug tests passed: TestTerrainSprites, TestTerrainEdges, TestTerrainSpritePacks, TestTerrainWalls, TestTerrainDoors and TestClock. Tests include original scatter fingerprints, exhaustive wall-fit combinations, transient door state and symmetric door variants, plus actual door/warning atlas import.

Six changed application translation units passed Clang 18 syntax checks with the project's strict warning settings: MapCanvasRoomDrawer.cpp, Textures.cpp, mapcanvas.cpp, mapcanvas_gl.cpp, mapdata.cpp and mainwindow.cpp.

Actual production shaders and imported atlases passed 1,754 render assertions each on Mesa desktop OpenGL 4.5 and OpenGL ES 3.2. The included door image is a shader/atlas check, not an application screenshot.

No complete application link, macOS/Qt 6.8 build, live server session or interactive clock-control test was performed here. TestClock initially required the extracted Qt plugin path; it passed after that environment correction.
