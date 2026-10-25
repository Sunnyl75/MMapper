# Organic terrain roadmap

This list preserves the requested refinements and separates them from the
preliminary water-reflection and clock-playback implementation.

## Appearance controls — requested 24 October 2026

- [ ] Grime opacity.
- [ ] Reflection brightness and normal-map strength, with useful defaults.
- [ ] Dawn/dusk colours and intensity; night colour and darkness.
- [ ] Colour progression and fading within the native dawn/dusk warning hours.
      Sunrise, sunset and the live game clock must retain MMapper's timings.
- [ ] Live preview and reset-to-default controls in graphics preferences.
- [ ] Settings changes update rendering parameters without rebuilding map
      meshes, rescanning rooms or regenerating material maps.
- [ ] Keep these as appearance preferences, not a complete authoring editor.

## Current preliminary work

- [x] Water-only reflectivity and optional generated-normal-map modes.
- [x] Generate material data from the colour artwork in a separate native
      C++ component, on a background worker; cache results by artwork/version.
- [x] No Python, SciPy or AI runtime in MMapper.
- [x] Pack normals and reflectivity into one additional data sample per
      seasonal water sample, using the existing terrain drawing pass.
- [x] Preview-clock playback: centre is normal forward game speed; immediately
      left is reverse; farther left/right accelerates; pause is separate.
- [x] Preview playback never changes the live clock, parser or game messages.
- [ ] Playtest visual strength and normal-map quality on Lewis's Intel Mac.
- [ ] Measure off / mask / mask-plus-normal GPU and movement-to-display costs.
- [x] Extend generated reflections to shallows pools, rapids and white mountain caps.
- [ ] Add optional authored-map overrides and per-material presets; consider
      other suitable surfaces after playtesting these defaults.

## Rendering correctness and art refinement

- [ ] Lintel/overlay at indoor-to-city boundaries.
- [ ] Refine eave corners and caps; keep existing wall confinement and
      squared city stone walls.
- [ ] Make city grime more apparent without hard doorway endings.
- [ ] Review water/road coverage, transitions, seams and distant tessellation
      after the water/road/current patch. Preserve fixed cave trails.
- [ ] Review the redesigned matching water-flow indicators in actual play.
- [ ] Investigate the intermittent native dark overlay, including Mewlip cave.
      Preserve the accepted behaviour where rooms own their sprite tint.
- [ ] Finish dawn/dusk playtesting, then isolate that feature into a master PR.
- [ ] Test whether small field foreground details benefit from shadows.
- [ ] Subtle large-area field variation suggesting changes in ground height.

## Artwork organisation and additions

- [ ] Consistent sprite names and a dedicated folder beneath pixmaps.
- [x] Material-pack identifiers reusable by multiple superzones, with fallbacks for every terrain family.
- [x] First Lórien mallorn, flowered ground and field-foreground pack, using the existing passes.
- [ ] Lórien-specific flets, roads and water artwork, with reviewed visual roles for flet rooms.
- [ ] Shire artwork: rounded doors and rush/thatch roofs.
- [ ] Rivendell artwork.
- [ ] More red autumn trees.
- [ ] Golden Mallorn trees that remain in leaf.
- [ ] Lock indicators where locked state can be discovered reliably.
- [ ] Roofs at distant zoom, except over the character's location.
- [ ] Appropriate mipmaps or replacement art for distant zoom levels.

## Maintainability and independent features

- [ ] Profile before broad optimisation; investigate map/mesh startup caching
      separately from the material-image cache.
- [ ] Refactor terrain into self-contained components and reconcile parallel
      sprite-placement strategies; preserve deterministic map-room-ID seeding.
- [ ] Stage the accepted implementation into dependency-ordered upstream PRs.
- [ ] Door-opening creak and closing click/slam sound triggers as a separate
      feature that can be proposed independently of organic terrain.
- [ ] Keep colour PNGs sufficient for creators; generated lighting data should
      be automatic, with optional supplied maps as overrides.
- [ ] Consider an optional pack validator/builder reusing the same generation
      and validation code; avoid maintaining a second rendering implementation.

The preceding indoor-track patch is a prerequisite of this branch state, not a
new unresolved task. The preliminary implementation does not claim that the
remaining visual refinements, full authoring workflow or Mac performance checks
have been completed.
