# Bundled background terrain

`background-terrain.tar.gz` contains one separate `background-terrain.mm2xml`.
It is expanded and checksum-verified by `cmake/BundledBackgroundMap.cmake`, then
embedded as a compressed, read-only Qt resource. The existing background loader
reads the ordinary XML map without a special terrain format or runtime converter.

The corrected dataset is version 5, updated 4 November 2026. Its 212,284 cells
include 21,563 retained surface cells from MUME/arda tag 42, with matching terrain
and visual road directions. Other cells use the aligned Fenlon-derived landscape.
The original artwork is not included. See `background-terrain.json` for source
commits, hashes, alignment and the omission policy, and `ARDA-NOTICE.md` for the
supplied public game-map attribution.

Users can select a custom background file or save an editable copy from
Preferences. Updating the application does not overwrite those separate files.
