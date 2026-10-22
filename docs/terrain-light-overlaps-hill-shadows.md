# Room-light overlaps and grounded relief shadows

Incremental to terrain checkpoint 19b07c2e and combined-token checkpoint f8dbefd9.
This follows the existing room-light halo implementation. It preserves the
separate twilight/clock correction and rapids/flow artwork updates.

## Fixed and carried lights

Both already share the same atlas, wall clips, mask and draw passes. The new
atlas keeps useful brightness in the outer halo rather than making almost all
of its extension transparent. Independent source brightness, animation phase,
rate and shape variation remain. The hue is consistently warm, avoiding colour
changes along a boundary where two flickering sources exchange dominance.

Illumination and shadow potential now use independent per-pixel maxima. The old
composition copied the brighter source's entire pixel, including its different
corner shadow. This caused a discontinuous handover even when the sources'
brightnesses differed by only one byte. The result remains order-independent
and bounded, so neighbouring pools do not add excessive brightness.

Shadow potential fades continuously with the circular mask. Cached wall
proximity restricts it to real blocking edges rather than the owning room's
square. Open joins no longer invent square corner shadows. Illumination from
either source suppresses the resulting shade. No shadow is painted beyond the
reach of the mask. Closed rooms retain flickering shade near their walls.

The blocking policy is unchanged: a missing exit or static door on either side
blocks, unknown neighbours are solid, and diagonal corner pinholes are blocked.
The one-texel guard remains to prevent filtered light crossing a wall. This
patch does not infer that a missing outdoor exit is visually open, nor change
which descriptions qualify as fixed sources. LIT alone still does not qualify.

Wall proximity is calculated with the existing lazy clip and cached alongside
its reachability bits. Each cached clip grows from 128 to 1152 bytes, plus the
existing map-container overhead. There are no new map scans, movement handlers,
timers, threads, textures or draw passes. The existing mask texture and 84 ms
flicker cadence are retained. Zero flicker continues to use the cached result.

## Hills and mountains

The tree-style projection rotated the entire illustrated silhouette across
the light direction. A wide or asymmetric hill therefore had its peaks moved
to different sides of its shadow. A short projection could also sit under the
image without an exposed ground contact.

Hills and mountains now use a shallow elliptical ground footprint swept along
the existing sun/moon vector. The original east/west silhouette supplies the
profile, including twin peaks. The contact footprint keeps shade visible below
the illustrated front during backlighting. It stays finite with light directly
east or west. Five bounded alpha samples per seasonal image reuse the existing
shadow draw pass, with explicit gradients and a softened mip footprint.

Tree and low brush projection, sprite placement, size, room seeds, seasonal
selection and directional face shading are retained. Wall clipping and indoor,
dark-room and permanent-shade exclusions still apply. No new hill artwork,
geometry uploads or changes to master clock/map code are introduced. This is
an illustrated relief approximation, not a 3D reconstruction of the artwork.

## Validation

The delivery README and verification.json record the focused Linux checks,
before/after mask timings and platform limitations. A production-shader
comparison using the actual Qt-packed hill sprites is included for review.
It is a test scene, not an application screenshot.
