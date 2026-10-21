# Static decorative doors

Live door opening/closing has been removed. Door artwork always uses the closed
frame, retaining saved-room seeded variants, matching paired halves, one-way halves
and the final overlay order above wall/eave decoration. This is a decorative marker,
not an indication of whether a door is currently open or closed in the game.

The terrain renderer no longer observes Room.Info/Room.UpdateExits for doors,
requests door-state redraws, stores server door observations, packs observed room
IDs into sprite instances, or uploads a live door uniform. TerrainDoors.h now only
contains the symmetric static variant selector; TerrainDoors.cpp is removed.
The existing PNG sheet is retained unchanged; its open frames are no longer selected.

MMapper's original exit/door discovery, mapping and path-matching behavior remains.
That existing map-update path, not the removed visual observer, produced the
SetExitFlags/no-change comparisons in the supplied log. Removing the visual feature
therefore does not eliminate those map comparisons or unsaved-room highlight scans.
The lighting observation is separate and retains its movement-follow fix.
