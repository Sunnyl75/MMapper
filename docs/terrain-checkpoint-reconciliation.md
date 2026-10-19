# Reconciled terrain on the preserved preview-clock checkpoint

The authoritative installation base is `bdd0b7b61bafc308e519444c94224c2ee5c78609` in `Sunnyl75/MMapper`, branch `feature/organic-seasonal-terrain-clock-checkpoint`. Its parent is `6a0ae0ec08d3add95c0f0eb425ed04be9f807799`. Use only `mmapper-terrain-bdd0b7b6-final.patch` on that checkpoint; historical V5/V6/V7 and incremental wall/city/eave installers overlap and must not be stacked.

## Reconciliation

The later terrain source was recovered from the current full V7 patch and verified to reproduce tree `57537ed613dd1a9b5b55ee7c2c8f74c9fc31a155`, the tree of the last terrain refinement commit `c68d8b80c1c41eee1ba9966b8e471e75234b0bac`.

The checkpoint versions of `mapcanvas.cpp`, `mapcanvas.h`, `MapCanvasWindow.h` and `mainwindow.cpp` remain byte-identical, preserving the preview-clock dock, timer, controls and rendering override. The checkpoint definitions of `TerrainSprites::forest`, `ground` and `field` also remain exact. Tests fingerprint their layouts over 4,099 room IDs per family. New wall fitting can move anchors near blocking walls; that is intentional. New water layouts use the preserved forest generator with independent hashing. Latest shadow lengths/opacities are retained, rather than reverting the accepted later wall-shadow behavior.

## Included behavior

- Seasonal and superzone-aware forest/field art, snowy winter, upright field details, and additional mountain, hill, brush, marsh, water, tunnel and cavern materials.
- Organic water under adjoining land, broad roads and connected trails; background wall clipping and whole-sprite foreground fitting.
- Underground wall overlays and indoor solar-shadow exclusion. Missing wall art adds no substitute overlay; the normal MMapper wall renderer remains available.
- Seamless city cobbles and indoor paving, timber interior faces and squared stone exterior faces, rough staggered wooden shingles, grime, diffuse shadows and end finishes.
- Final authored decorative roof hip/valley caps, overlapping the straight strips in their own final batch. Opaque cut backing removes thin transparent seams; narrow feathered gutters avoid rectangular corner outlines. These are the latest implementation, with visual review still pending.
- Required PNG resources, shader changes, focused tests and the optional production-renderer review fixture.

The original underground wall additions are included in this version. The old `mmapper-wall-overlays-v1.patch` is neither required nor an alternative implementation. Historical documentation elsewhere in this tree explains development stages; follow this document for installation and checkpoint status.

## Apply and validate

Start with a clean checkout and create a branch at the exact checkpoint. Run `git apply --check` before applying the new binary patch. Build using the normal Qt/CMake configuration for your platform. The reconciliation was validated on Linux; it has not been built on the user's Mac. See the accompanying final guide for exact commands and fresh build/test results.
