# Underwater terrain and wall-side coverage follow-up

Apply this increment to the working reconciled terrain version based on the bdd0b7b6 checkpoint, with its strict-compiler fixes. It includes the small city-no-trail correction; the accompanying installer accepts either state of that earlier correction. Do not reapply V5/V6/V7 or the large checkpoint reconciliation patch.

## Active rendering changes

- UNDERWATER has its own sector/families, separate from surface water and marsh. Six fixed sediment/stone ground patches and twelve fixed aquatic plant/rock details are normalized from their source grids into the existing atlas layout. Four foreground details are selected deterministically from saved map room IDs. There is no winter replacement, surface-wave art or sky-shadow pass underwater.
- Brush uses the existing saved scatter, with its size multiplier increased from 1.05 to 1.55. A 0.90-room ceiling keeps whole shrubs small enough to fit between opposing walls. Existing trail clearance and wall relocation still apply.
- Natural ground has opaque, world-anchored textured underpaint sampled from the interior of its own material. Scattered ground retains its irregular fringe, but wall planes no longer cut a 0.025-room empty setback. Pixels beyond a blocked boundary are still rejected. Ordinary sprite sampling uses explicit pre-clip derivatives and mip-aware atlas guards.
- A road's verge no longer borrows colours or decoration from neighbours behind walls. On the wall side, road material continues to the boundary. Open boundaries retain their original irregular overlapping land fringe.
- City paving suppresses the legacy trail overlay. Map exit/trail data is unchanged.
- Rounded outside roof-cover artwork replaces the pointed caps; source layout, joining shoulders, atlas roles, overlap and draw order remain unchanged. Inside valley roles are retained in the edited sheet.

## Included artwork

- `terrain-underwater-ground.png`: 3 columns x 2 rows, six fixed variants.
- `terrain-underwater-details.png`: 6 columns x 2 rows, twelve fixed variants.
- `terrain-roof-caps.png`: 3 columns x 2 rows, outside/inside corner roles.
- `terrain-doors.png`: 6 columns x 2 rows; six designs, closed above and open below. It is registered as a resource for the next door integration but is NOT drawn by this patch. No unused door texture-array layers are allocated.

The underwater art brief is a clear, hand-inked fantasy comic style with light turquoise-grey silt, slate stones and olive plants, compact irregular background patches and matching sparse foreground detail. The roof edit shortens the top-row diagonal tips into rounder fans while retaining the shoulders. Door variants cover plain, weathered, pale, iron-strapped, painted and iron-bound timber, with overhead closed/open views. These assets were generated using the built-in image tool.

The door sheet does not by itself implement matching half ownership or live opening. Those require a shared doorway identity and temporary observed state, including an explicit unknown state for unobserved remote rooms. Current legacy door symbols are preserved until that integration is complete.

## Compatibility and verification

Existing material family indices are unchanged; two underwater families are appended. The maximum texture array grows from 140 to 144 layers. Compact fixed-role normalization happens at load time and introduces no dependency, saved-map field or new map format. Preview-clock integration and the original forest/ground/field scatter functions are unchanged.

Focused Clang Debug compilation/tests, production shader checks on desktop GL and GLES, asset normalization and exact patch application are recorded with the package. This update was not followed by a full application build or a macOS build.
