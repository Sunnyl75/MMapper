# Terrain folders matched by area name

This is a **one-time generic loader extension** on superzone-packs-v1.
Adding or changing an area afterwards is an artwork-only operation.

## One-time installation

Extract this ZIP into Downloads as `mmapper-terrain-name-lookup-v1`:

```bash
cd /Users/Lewis/Projects/Master15Apr/MMapper
python3 "$HOME/Downloads/mmapper-terrain-name-lookup-v1/apply-terrain-name-lookup-v1.py" --check &&
python3 "$HOME/Downloads/mmapper-terrain-name-lookup-v1/apply-terrain-name-lookup-v1.py"
```

Build the same project in Qt Creator once. Existing bundled and manifest-based
packs continue to work. This is compatible with either having applied or
skipped the earlier Redhorn source-art installer; it does not change that
installer's area mapping or QRC registrations.

## Routine artwork updates

Put terrain folders under **the same assets or user resources root that holds
your `areas`, `rooms` and `sounds` folders**. For Redhorn, the layout is:

- `terrain/redhorn-pass/terrain-mountain-ground-seasons.png`
- `terrain/redhorn-pass/terrain-mountain-peaks-seasons.png`

The separate `mmapper-redhorn-drop-in.zip` supplies that layout ready to copy.
Restart MMapper after adding or replacing image files. A routine art update
requires **no code, installer, QRC entry, JSON entry or rebuild**.

The root can be MMapper's existing sideloaded assets directory, or the resource
directory already configured in Graphics preferences. Do not put the files in
the source `src/resources` tree for this workflow. On macOS use the runtime
location that already holds your media; the code reuses `getAssetsPath()`.

## Matching and fallbacks

The room's saved Area is converted using exactly the media convention:
lowercase, remove leading `the` plus whitespace, replace spaces with hyphens,
and use MMapper's existing ASCII transliteration.

Examples: `the Redhorn Pass` → `redhorn-pass`; `Lorien` and `Lórien` → `lorien`.
The unrelated `the Lorien Surroundings` → `lorien-surroundings` and is not
matched by a `lorien` folder. Blank Area values still select normal artwork.

User resources have priority over sideloaded assets. Each file falls through
the ordered roots independently, then through existing bundled/global defaults.
An invalid higher-priority image falls through too. Unprovided families retain
normal art. All existing tree and terrain sheet names/layouts are supported.

Folders are discovered once when terrain textures load. No folder scans or
image generation happen on movement or during flicker frames. Area lookup
uses the prepared in-memory pack selection.

## Optional shared packs

No manifest is needed for ordinary named folders. To deliberately share one
pack across several named areas, the existing `superzones` mapping remains
available in an optional `terrain/packs.json`:

```json
{"superzones":{"Area One":"shared-alpine","Area Two":"shared-alpine"}}
```

Put its images in `terrain/shared-alpine/`. Explicit mappings keep their
existing exact saved-Area spelling. A directly matching named folder takes
priority, allowing one area's art to be customised without changing a shared
pack. User alias entries override asset-root alias entries. The existing
44-distinct-pack and texture-layer limits remain in place.

## Scope

The patch changes four existing terrain/resource implementation files, their
existing test linkage and adds one native loader test. It does not change the
media implementations, placement, clocks, lighting or shaders. See
`VALIDATION.md` for tests and build limits.
