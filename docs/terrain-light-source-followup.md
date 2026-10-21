# Street-lamp contents and recorded LIT fallback

This increment applies after circular light halos and the street-lamp clock patch.
It changes source classification only; masks, flicker, clipping, movement and the
preview-clock connection remain unchanged.

`street-lamp`, including the hyphen, was already recognised, and the clock tester
was already connected. However, classification read only the static description.
The update also checks saved RoomContents specifically for street-lamp mentions.
These object lines can identify a fixture even when it is absent from the prose.
Other contents (carried torches, dropped candles, campfires, etc.) are deliberately
not treated as permanent sources. Names are still not scanned.

The broad recorded-LIT fallback introduced by this follow-up has subsequently
been removed by the dark-room/sprite patch: LIT may describe sunlight and cannot
establish an artificial source in any sector. Explicit fixture evidence and
GMCP artificial-light observations remain independent of that flag. A carried
light is not latched after the server changes it; doing so would incorrectly
illuminate later genuinely dark rooms.

The requested street-lamp exception remains: outdoors it uses native
DAWN/DUSK/NIGHT, indoors it stays illuminated. An outdoor street-lamp room follows that schedule unless its prose establishes
another active permanent source. Preview time continues to drive the same native states.

Classification still happens once at initial map load and then only for the room
entered. There are no extra frame-time map scans or new event/prompt processing.
Restart/reload the map after rebuilding so the new rules reach cached rooms.
If a lamp is missing from both the saved description and contents, this heuristic
cannot discover it without learning the room; a real example is then needed.
