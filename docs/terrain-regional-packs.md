# Superzone overrides for every organic terrain family

This completes regional selection beyond the original tree-only support.
It extends the existing `organic-terrain/packs.json` mechanism. It adds no
scatter algorithm, flower pass, clock override, room metadata or live scan.

## Selecting and sharing a pack

The existing manifest associates exact saved room area strings with pack
directories:

```json
{
  "superzones": {
    "Lorien": "lorien-mallorn",
    "Lórien": "lorien-mallorn"
  }
}
```

Local manifest entries override the matching bundled entries; they preserve
bundled mappings for other areas.

The manifest lives at `pixmaps/organic-terrain/packs.json` in the configured
resources directory, sideloaded assets or bundled resources. Case and accents
are significant. The published map's key is `Lorien`; the separate
`the Lorien Surroundings` area is not included.

Several area names can point to the same directory. One selection and one set
of atlas pages are shared by those aliases. The limit is 44 distinct packs; aliases do not consume additional pack
slots. A missing area uses the normal default selection. The bundled
accented `Lórien` key is a compatibility alias; the published XML key actually
verified in this work is the unaccented `Lorien`. Both aliases share one pack.

## Overriding artwork

Place an optional replacement file inside the selected pack directory, using
the same filename and format as its normal counterpart. Every family in
`TerrainMaterials.h::SHEETS` is supported: ground and foreground sprites,
water/rapids, trails/roads, borders and shores, city/indoor paving, underground
materials, walls/eaves/finishing pieces, underwater materials, doors, hazards
and water-flow indicators. `terrain-roof-caps.png` can also be replaced there.

For example:

```text
pixmaps/organic-terrain/lorien-mallorn/
    spring.png
    summer.png
    autumn.png
    winter.png
    terrain-forest-ground-seasons.png
    terrain-field-ground-seasons.png
    terrain-field-details-all.png
```

Trees retain their original 3-column × 2-row seasonal file layout and
`all.png` option. Other families retain their existing native source formats:
seasonal sheets are 6 × 4; repeating paving is square; doors, underwater sheets,
hazards and flow indicators use their existing fixed layouts. Wall/eave role
rows are preserved rather than interpreted as new seasons.

For a seasonal family whose filename ends in `-seasons.png`, an optional
`-all.png` file supplies six all-year variants in a 3 × 2 grid. For example,
`terrain-field-details-all.png` replaces grass tufts with six flower clumps.
The loader expands the six cells into the existing seasonal atlas format;
there is no new render path. A valid regular seasonal filename takes priority.

Family fallback is local pack artwork → bundled artwork in the same pack →
normal global/custom/bundled family artwork. An invalid regional image is
reported and falls back to the normal family. Families not overridden share
their existing pages instead of loading a complete duplicate terrain set.
Trees use local season → local all-year → bundled regional season/all-year →
existing global seasonal/all-year/bundled fallback.

An existing external tree-only `lorien-mallorn` pack can therefore continue
supplying its trees while using the newly bundled regional floors and flowers.
An external manifest can also name a bundled pack without copying its folder.

## Lórien artwork and passes

The bundled pack has six silver-trunked mallorns: green/silver foliage with
gold blossom in spring/summer and retained gold leaves in autumn/winter. The
season pairs share resource aliases, preserving the same tree identities
without duplicating their PNG bytes in the source tree.

Elanor and niphredil are painted into the forest ground patches. Field ground
initially shares that green/gold ground artwork through a resource alias.
Fields use the existing foreground pass with the flower clumps instead of the
usual grass tufts. Forests continue to draw only trees in their foreground pass.
There is no extra flower layer, family, draw call or separate flower scattering.

The normal seasonal transition and time controls remain active. Regional
underpaint colours are derived once from the corresponding artwork, so a
deliberately green winter background does not expose the generic snowy fill.
This does not force spring time or alter daylight/sunrise/sunset.

No regional city/flet, road, shallow-water or ordinary-water replacement art is
provided yet. Those families inherit normal artwork until their files are
added. Actual flets occur in multiple sectors; this patch does not reclassify
rooms or assume that every CITY room is a flet.

## Renderer and performance

`TerrainSpritePacks.cpp` owns material preparation, family fallback and the
resolved selections. Selection happens while textures/meshes are prepared.
There are no filesystem or description scans during movement or flicker.

Instances carry either their selected atlas or a small selection-row index.
One shared metadata page resolves the indirect material reads used by road
joins, banks and underpaint. It also connects each reflective colour sheet
with its generated normal/reflectivity pages. The existing low-priority,
content-cached material-map worker prepares regional replacements too.

The texture loader uses the device's array-layer limit, capped at the existing
16-bit tile-address range. It reserves reflection and lookup pages before
accepting replacement families. If a collection exceeds that limit, it reports
the affected files and retains their normal artwork. Missing/unsupported wall
art remains absent. This avoids overflowing low-limit OpenGL/GLES devices.

Paired door halves share the same deterministically selected owner pack even
when they straddle a superzone boundary. Door observations, networking and
movement timing are unchanged.

## Validation

`TestTerrainRegionalPacks` loads the actual native formats for all 32 material
families, tests aliases/exact keys, invalid/missing fallbacks, all-year flowers,
metadata/reflection mappings and layer-budget fallback. Existing tree/wall and
material-map tests remain applicable. `regional_packs_render.py gl|es` exercises
the production shaders' regional ground, inherited road, repeat paving,
regional reflections, indoor exclusions and green winter underpaint.
