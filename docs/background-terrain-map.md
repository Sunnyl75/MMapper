# Background terrain map

A separate, optional terrain map is drawn beneath the playable map. Enable it
under **Preferences → Background Map**, choose a `.mm2` or `.mm2xml` file, and
leave your usual game map open in the main window. Both files use the same
MMapper coordinates; there is no image registration or guessed offset at runtime.
The initial default is 40% opacity, softened tile detail, and a foreground shadow.

## Files and editing

The background is read-only **in this layer**. It cannot be selected, moved,
deleted, matched to game output, included in paths, or saved into the game map.
Loading it does not change foreground position, bounds, history, filename, or
modified state. Names, descriptions, infomarks, objects and exit overlays are
not displayed. Terrain and directional road flags are the only displayed data.
Only background rooms on the currently viewed elevation are drawn.

To modify it, open the background file as an ordinary map in a separate MMapper
window, edit and save it, then press **Reload** on the Background Map preferences
page. Keep the foreground and background as separate files. Native `.mm2` saves
use the application's current format; the normal map readers also accept the
generated `.mm2xml` files. CSV is not a map format accepted by this layer.

## Tile packs and appearance

- **Built-in tiles:** the bundled standard artwork, independent of game-map
  resource overrides.
- **Current game tile pack:** a snapshot of the resource paths selected for the
  game map when the background tiles are loaded. Later foreground refreshes do
  not reload it. Press Reload to take an updated snapshot.
- **Separate custom tile pack:** choose a resource-pack directory containing
  `pixmaps/`, or the `pixmaps` directory itself. Standard `terrain-*.png` and
  directional `road-*.png` names are supported. Missing files fall back to bundled
  tiles. This selection does not change the game's resource directory.

The opacity slider is live and persistent, from 0% to 100%. It changes a shader
uniform, without reading files, decoding images, rebuilding geometry, or uploading
textures. At 0% neither the background nor its foreground shadow is drawn.
Disable Show background map to retain the cache while hiding it.

Softening reuses the existing portable image blur on the small cached tile
textures. It softens artwork detail; it is not a full-screen blur across terrain
boundaries. The optional shadow is a diffuse skirt along exposed foreground
room edges, offset southeast in map coordinates. Adjacent foreground rooms have
no shadow seam along their shared edge. The focused playable terrain is opaque
when the background layer is enabled with nonzero opacity.

This branch is based on upstream master, not the separate organic-rendering
branch. It supports standard image tile packs. Seasonal sprite scattering,
organic terrain materials, and the conceptual mockup's illustrated buildings
are not added by this patch. A future organic renderer can consume the same
read-only terrain data without changing the file or preference model.

## Refresh and cache boundaries

| Event | Background file read | Background geometry rebuild | Background tile upload |
|---|---|---|---|
| Foreground refresh, edit, movement or save | No | No | No |
| Pan, zoom, resize, elevation or camera rotation | No | No | No |
| Opacity or shadow visibility | No | No | No |
| Tile-pack selection, custom directory or softening | No | No | Once, after worker preparation |
| Background file selection or explicit Reload | Once | Once | Reload also rereads tiles |
| GL context recreation | No | No CPU rebuild | Restore from CPU cache |

Every painted frame composites the already prepared geometry in the current
camera position. That GPU redraw is necessary for a shared canvas, and does not
mean that the background content has been refreshed. Off-screen 64×64-room
chunks are culled; visible chunks get a GPU mesh once and retain it. No file
watcher, per-frame image rasterization, full-map framebuffer or per-room texture
allocation is used. Tile changes do not discard background mesh objects.

The foreground silhouette has its own asynchronous shadow cache. A new foreground
map snapshot may rebuild that silhouette only; it cannot invalidate any of the
background caches. Panning and zooming do not rebuild the silhouette.

## Implementation and portability

- `display/BackgroundMapLoader.cpp` reuses the existing format detector/readers.
  It reduces loaded rooms to display geometry; it does not create a second
  editable `MapData` or retain gameplay metadata.
- `display/BackgroundMapData.*` contains CPU-only geometry, image preparation,
  shadow construction and projection-based culling.
- `display/BackgroundMapLayer.*` owns independent caches, generation checks,
  task cancellation, GPU resources and context restoration. Stale worker results
  cannot replace a newer selection; callbacks check QObject lifetime.
- `preferences/BackgroundMapPage.*` owns the controls. They use specific named
  settings callbacks, not the general graphics-change signal that remeshes the
  foreground.

The implementation uses Qt 6, the existing async task system, existing blur and
existing instanced texture-array shaders. It introduces no platform-specific
graphics API or new library. CPU loading/preparation stays off the GUI thread;
texture and visible-chunk uploads happen with the canvas GL context current.
OpenGL 3.3 and the existing GLES 3/WebGL 2 path use the same data.

On desktop, Browse keeps the selected map's external path. The browser picker
imports the file into the browser's virtual filesystem. Browser custom packs
must already be deployed in that filesystem; arbitrary host-directory browsing
is not available. Persistence of imported browser files depends on the host's
filesystem mounting/synchronization, and is not newly implemented here.

## Review checks

Use separate test foreground and background files. Check that loading the
background leaves the foreground's modified state and room count unchanged.
Select/delete a room in an apparent gap: background terrain must not be selected.
Change a foreground room, save, move and refresh: background loading/preparation
must not run again. Change opacity, pan and zoom: cached artwork must stay aligned.
Change only the background pack: foreground artwork must remain unchanged.
Edit the external background and press Reload to see that change.

`TestBackgroundMap` covers negative-coordinate chunking, elevations, directional
roads, duplicate positions, cancellation, exposed-edge shadows, projection culling,
cached image independence, optional softening, preference round-trips/clamping,
and opacity changes leaving content settings untouched. Run with:

```sh
ctest --test-dir build -R TestBackgroundMap --output-on-failure
```

Native Windows/macOS execution and a browser build still require platform CI or
manual review; using their shared code paths does not itself establish those
platforms have been tested.
