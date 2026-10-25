# Terrain moonlight

Adds pale blue illumination to outdoor organic terrain at night and uses the
same moon position, phase and horizon envelope for existing specular highlights.
It extends the reflections v2 terrain build; it is independent of token support
and of the optional named-area-folder loader.

## Behaviour

- Uses MMapper's native MUME moment, lunar zenith and phase, including the
  preview clock. No separate real-world clock or astronomy model is introduced.
- Full moons are strongest; intensity follows phase squared and fades near the
  horizon and at the boundaries of NIGHT. The accepted one-game-hour DAWN and
  DUSK solar palettes are unchanged.
- Adds a pale blue directional fill while preserving authored texture detail,
  ink and transparency. Upright terrain uses the same rounded relief proxy as
  existing self-shading; it is not a generated per-sprite normal map.
- Reuses the existing material reflection masks and normal maps. Moon highlights
  are deliberately weaker than sunlight; no new image assets are needed.
- Existing NO_SKY_SHADOW exclusions apply: indoors, tunnels, caverns,
  underwater and dark/permanent-shade rooms receive no moon fill or glints.
- Existing artificial-light coverage suppresses the cool fill, preserving warm
  room-light pools. The night/weather overlay still composites over the terrain.
- Time-of-day intensity controls the fill; zero disables it. Reflection mode and
  reflection brightness independently control glints. Fill works with reflections
  disabled or while material maps are pending.

## Manual test in the existing application

1. Apply the patch and rebuild the normal terrain project in Qt Creator.
2. Enable organic terrain and the time-of-day effect; use a normal outdoor room.
3. Open the existing preview clock, enable preview time and Pause.
4. Set Year **2850**, Month **1**, Day **1**, Hour **0**, Minute **0**.
   MMapper's epoch gives a visible full moon at its zenith at this instant.
5. Compare with a daytime hour. At night, terrain should have a pale blue fill.
   Enable Terrain reflections / Mask and normal map to see moon-coloured glints
   on existing reflective materials. Existing reflection brightness still applies.
6. Move preview time through the night: direction changes, with horizon fades.
   Try a new-moon date or a moon below the horizon: its extra light disappears.
7. Indoors and permanently shaded rooms should retain their existing appearance.
   Warm fixed/character-light pools should not be washed blue.

Preview changes do not alter MUME server time. Exact visual intensity on the
Mac still needs playtesting; this patch has not been benchmarked on that GPU.

## Cost and scope

One existing sky-source sample supplies direction, reflection colour and moon
strength per terrain draw. One additional uniform and a gated fragment-shader
blend add the light. There are no additional draw passes, sprites, map scans,
lighting-index rebuilds, texture uploads, GMCP requests or animation timers.
Enabled fragments do add shader arithmetic; this is not a claim of zero GPU cost.

## Files and verification

TerrainReflections.h owns the shared envelope; mapcanvas_gl.cpp supplies it;
OpenGLTypes.h and legacy/Shaders.h carry the uniform; terrain/frag.glsl applies
it. TestTerrainMoonlight checks the sky model. moonlight_render.py exercises
production desktop GL and GLES shaders, including the actual night overlay.
The existing Python terrain renderer uses bytes(instance) for compatibility
with current PyOpenGL. No artwork or saved map/config format changes occur.
