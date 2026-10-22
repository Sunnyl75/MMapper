# Check whether MUME reports live door changes

This is an optional observation trace, not an open-door renderer. Enable with
MMAPPER_DOOR_TRACE=1 before launching MMapper. With it unset or zero, no extra
GMCP subscriber is connected. No persisted preference is created.

The trace records received Room.Info and Room.Update.Exits messages. Room.Info
is reduced to its ID and exits; partial exit updates are retained intact so we
can establish their actual format. Other GMCP messages, including login and
character information, are excluded. The prefix is [door-trace], followed by
monotonic elapsed milliseconds, message name and compact JSON.

No map writes, mesh invalidation, repaint requests, polling timers or commands
to MUME are introduced. Logging overhead applies only while enabled. This
test establishes server message behaviour; it is not a gameplay performance
benchmark. Master's existing parser and mapping work are unchanged.

## Test

1. Stand beside a known closed door. Use look once for a baseline Room.Info.
2. Open that door, then wait about three seconds without looking or moving.
3. Close it, then wait again without looking or moving.
4. Note whether Room.Update.Exits or an unsolicited Room.Info arrived after
   each action, and whether the relevant exit's flags changed.
5. If neither action produced an exit update, open once more, wait, then look.
   This distinguishes automatic updates from updates caused by a fresh look.
6. If convenient, repeat with someone else opening/closing it while you remain
   still. That tests whether other players' actions are reported to you too.

Use a door you can operate without a key. Substitute its actual direction or
keyword in your commands; no game commands are issued by this diagnostic.

MMapper's existing full Room.Info parsing treats a named door without the
closed flag as open. We must inspect partial updates before assuming that an
omitted flags field means the same thing. The trace reports data without
inferring door state or changing its drawing.

If updates arrive automatically, we can drive the proposed door-panel removal
from those observations using a small cached drawing value. If changes only
appear after look/movement, an immediate update needs another reliable server
signal; we should retain the last confirmed appearance rather than poll or
assume that a sent command succeeded.

Quit after the test. A normal Qt Creator launch without MMAPPER_DOOR_TRACE
returns to the normal behaviour, even with this patch installed.
