# Dark-room tint, overhangs and artificial-source evidence

This increment follows the source-contents/LIT follow-up. It removes both forms
of recorded-LIT inference: neither an enclosed nor an outdoor room becomes an
artificial light solely because its Light flag says LIT. That flag may describe
sunlight. Active fixture descriptions, street-lamp mentions/schedules and live
GMCP artificial-light observations remain. Stored map flags are never altered.
Reload/restart after rebuilding to discard the earlier derived light index.

The later sprite-tint preference reversal restores the earlier behavior: each
sprite carries its owning room's DARK/NO_SUNDEATH tint over its entire image,
including overhangs outside the room square. The image and alpha silhouette are
not clipped. A normal neighbouring sprite still does not inherit a dark room's
tint just because it overlaps it. Night/dawn/dusk weather and actual artificial
light relief remain separate and unchanged. No LIT-based artificial-source
inference is restored.

The native room-tint pass uses an EQUAL depth comparison. Its vertex shader and
our organic terrain vertex shader now both declare gl_Position invariant. This
requires compatible position results when their ground quads coincide, avoiding
a driver-dependent risk from independently optimised shaders during panning.
The depth test, native map update/path logic, render ordering and draw count are
unchanged. The reported Mac panning symptom has not been reproduced here; removing
false sunlight-derived lights also removes one cause of unwanted dark-tint relief.

Regression tests render:
- a dark-room tree with a north overhang: both the inside and overhang retain
  the owning-room tint, and the entire alpha silhouette is identical;
- the same ownership behavior under panning and changed light-mask origins;
- actual organic ground followed by the native dark overlay using a depth buffer
  and the production EQUAL test, under panned/tilted matrices, with lighting off,
  with a zero mask, and with the room outside the cached light grid;
- LIT-only enclosed/outdoor/sunlit descriptions yielding no artificial source,
  while real fixture evidence, street-lamp contents/scheduling, wall clipping and
  the retained live-light lifecycle still work.

Desktop GL and GLES tests pass on Mesa; the Mac Intel driver and normal gameplay
still need the user's visual check. No new timer, frame-time scan, dependency,
asset, compilation unit or CMake entry is introduced.
