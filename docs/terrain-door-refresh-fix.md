# Fix map clearing and rebuilding on live door observations

Base: source tree b0859a4ee25d4652e08f118e7df62bd2679eff5a, published as cb02a38557176178d1ef639528c6ef9ab8679584. The patch also applies when those same terrain files are uncommitted over bdd0b7b6; synchronisation is not required.

## Cause

The connected/Event.Moved/Room.Info/Room.Update.Exits handlers called requestForceUpdateMeshes for transient door changes. The next paint discarded existing map meshes and marked pending mesh work to be ignored. Movement and the following Room.Info could repeat this. With the reported approximately 11-second terrain build, the map could remain blank through ordinary movement. This invalidated the rendering cache, not saved map room data.

## Fix

- GMCP door changes and connection/reset events request a redraw only. They never discard terrain meshes or pending terrain results.
- Door instances contain static identity: owning server room ID, paired peer ID, direction and saved-map-ID-seeded style. The owning/peer server IDs are only observation keys; artwork still seeds from saved map room IDs.
- Four integer draw-uniform words contain the current observed room and packed per-direction states. The vertex shader chooses closed/open atlas cells and known/unknown brightness each frame from those words. The instance buffers stay unchanged when moving or opening a door. Paired reverse directions share observations; a one-way door has no peer observation key.
- Exact signed/unsigned integer bit transport preserves the full 32-bit ID range. IDs never pass through floats and never index the named-colour array.
- Transient door state is removed from MapData::generateBatches, generateMapDataFinisher and the async builder options. Ordinary map edits, loading, texture changes and rebuild controls keep their original update paths. Normal asynchronous map changes may still rebuild meshes; the existing cached map remains available during that work.

No artwork, clock, seasonal/scatter rules, terrain placement, saved-map format or GMCP forwarding is changed. The fix adds no per-door geometry or extra draw pass. This does not optimise the cold full-map build itself.

## Validation

Five focused Linux Debug tests passed: TestTerrainSprites, TestTerrainEdges, TestTerrainSpritePacks, TestTerrainWalls, TestTerrainDoors. Door tests cover observation/reset state, integer packing and high 32-bit IDs.

Production Clang 18 syntax checks passed for MapCanvasRoomDrawer.cpp, mapcanvas.cpp, mapcanvas_gl.cpp, mapdata.cpp and opengl/legacy/Shaders.cpp, using project compile flags. Existing non-fatal warnings remain. Builds used ccache and the project sanitizers; leak detection was disabled for container execution.

Production shader tests with actual imported artwork passed 2,025 assertions each on Mesa desktop OpenGL 4.5 and OpenGL ES 3.2. New regression checks change only the observation uniform while reusing the same VBO: closed/open/closed, movement-to-unknown, unrelated observation, paired reverse observation, four directions, full 32-bit IDs and one-way isolation. Existing terrain/artwork checks still pass.

Installer check/application/repeated-application detection and exact forward/reverse tree validation are recorded in verification.json. No full app link, macOS build or live connected gameplay test was performed here.
