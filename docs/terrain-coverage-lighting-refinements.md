# Full-room roads, directional foreground lighting, warnings and roof corners

Required baseline: the already-applied mmapper-terrain-joins-doors increment, tree `09ac6ba201735384d2abdbe55ecffe48cb28009d`. This is one new incremental patch. Do not reapply earlier cumulative patches or the joins/doors patch.

## Rendering changes

1. ROAD rooms use the road material across the complete room instead of a corridor over interpolated neighbouring land. This removes the broad flat green strips seen between the actual grass imagery and the road. Adjacent terrain sprites supply the fringe; the existing wall cuts and narrow irregular overlap remain. ROAD rooms without trail flags also receive this material, even if no legacy road texture exists. Trails remain narrow and keep their road join. City cobbles are unchanged. Water already had a full-room opaque base with overlapping water patches; a new regression check explicitly verifies full water-room coverage.
2. Trees, mountains, hills and brush receive a shared directional foreground shading model using the current sun/moon vector. A rounded normal proxy darkens the visible south-facing surface under northern backlighting, with upward-facing crowns/summits retaining more light. Ambient fill retains ink and colour detail. Moon intensity uses the existing lower opacity; indoor/no-sky rooms and non-casting details are excluded. This is an inexpensive approximation for painted 2D sprites, not geometric self-shadowing or inter-sprite occlusion. No normal-map assets, extra geometry, pass or random placement engine are added.
3. Death-trap warnings now draw above legacy room-load/mob icons, walls, roof caps and door artwork. Previously they drew before legacy icons, which could obscure them. Warning creation is also independent of the presence of a legacy sector texture. The existing DEATHTRAP load flag is the trigger; unmarked dangerous rooms are not inferred. The skull artwork is retained from the preceding package.
4. terrain-roof-caps.png is replaced with a six-piece sheet with rounded, shorter diagonal silhouettes. The existing top/left shoulders are anchored to the inner corner during import instead of re-centred when the silhouette's bounding-box proportions change. Their overlap with straight eaves is retained. The renderer limits the convex corner and its underlying return to a rounded footprint so a square underlay cannot protrude beneath the cap. Concave returns and room ownership remain intact.

The preview clock, seasonal scatter seeds, door-state observation, underwater packs, masonry walls and straight eave strips are preserved. No new dependency or map-format field is introduced.

## Artwork provenance

The replacement PNG was edited using the built-in image-generation tool, using the previous six-piece roof-cap sheet as the edit target. Final prompt: preserve the three-column/two-row equal-square layout, aged warm wooden overlapping shingles, top/left joining shoulders and comic ink style; pull back the lower-right diagonal silhouette into a compact rounded quarter-circle, with no pointed fan tip; top row convex corners, bottom row matching return pieces; isolated sprites with transparent gutters, no labels or background. Actual prompt also specified complete pieces within cells and matching wood colour/line weight. Renderer and atlas-import checks validate the final in-app footprint independently of the generated silhouette.

## Validation

- Five focused Debug terrain tests passed: TestTerrainSprites, TestTerrainEdges, TestTerrainSpritePacks, TestTerrainWalls and TestTerrainDoors. The two importer tests were rerun after the final shoulder-anchor adjustment and passed.
- Changed TerrainSpritePacks.cpp compiled under the project flags. Changed MapCanvasRoomDrawer.cpp passed Clang 18 syntax checking with the production flags. Existing non-fatal switch-enum warnings remain.
- Production shaders with actual imported roof, door and warning atlas pages passed 1,971 render assertions on Mesa desktop OpenGL 4.5 and 1,971 on OpenGL ES 3.2. New checks cover all road masks and walls, water coverage, back/front lighting, indoor exclusion, weaker moon shading, preserved alpha silhouettes, real warning artwork, shoulder overlap, rotation and absence of the diagonal cap tip/underlay.
- Linux Debug builds used ccache and the project's sanitizers. Leak detection was disabled for container test execution. An initial test executable permission issue was corrected in the test environment; the affected test passed on retry. No application source workaround was needed.
- Exact installer check/application/repeat detection and patch forward/reverse tree checks are recorded in verification.json.

No complete application link, macOS build, interactive screenshot or live server death-trap test was performed. Rebuild the existing Qt Creator target to review the integrated appearance. The previous clock is unmodified by this increment.
