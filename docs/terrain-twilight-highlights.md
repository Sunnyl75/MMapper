# Dawn and dusk highlight blending

The organic terrain shader now colours material highlights during the hour before
MUME sunrise and sunset. It uses overlay blending, weighted by material luminance,
so bright grass, pale rocks and snow catch the sky colour without washing out dark
ink. The existing time-of-day overlay still supplies the gradual night darkening.

The colour follows the existing smooth violet/rose/peach/gold dawn sequence and
gold/peach/rose/violet dusk sequence. A smooth strength envelope fades the new
material effect to zero at both ends of each transition. Daytime, full night and
an unknown clock keep their previous rendering. Seasonal sunrise/sunset times and
the preview clock are unchanged.

The existing Time of Day intensity preference controls both effects; zero disables
them. The configured night-opacity also scales the highlight effect, with its
reference opacity at 0.35. No new preference, timer, texture, asset or draw pass is
introduced. The shader adds arithmetic during twilight, and one cached light-mask
sample when room lighting is enabled. Actual performance still needs checking on
the target GPU.

Terrain already marked NO_SKY_SHADOW (indoors, tunnel, cavern, underwater, dark or
permanent shade) does not receive the new grade. Wall, roof, grime and projected
shadow artwork are excluded. Artificially illuminated outdoor areas suppress it
in proportion to their existing cached light mask. This patch does not change the
older full-screen sky overlay's indoor handling or light-source rules.

Scope: organic terrain materials and foreground sprites. Legacy room textures,
characters, labels and selections retain their previous rendering. No map-index,
movement, door, timing or preview-clock behavior changes.

The small CPU palette and strength envelope remain in TerrainTwilight.h. The
material blend and eligibility check are at the end of the terrain fragment
shader. Its uSkyTint uniform reuses the existing render-state slot: GLWeather
supplies a material grade to terrain, and the original sky tint to the final
weather pass. Nothing is cached in map geometry.

Validation extends TestTerrainTwilight and terrain_edges_render.py. The latter
checks the production shader on desktop GL and GLES, including ink protection,
alpha preservation, indoor exclusion, artificial-light suppression and disabling.
