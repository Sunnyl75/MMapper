#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""Rebuild the circular 16-frame light atlas; Pillow is needed only to regenerate it."""
from math import atan2, hypot, pi, sin
from pathlib import Path
from PIL import Image

SIZE, FRAMES = 32, 16
sheet = Image.new('RGBA', (SIZE * FRAMES, SIZE))
for frame in range(FRAMES):
    phase = frame * 2 * pi / FRAMES
    for y in range(SIZE):
        for x in range(SIZE):
            u, v = (x + .5) / 16 - 1, (y + .5) / 16 - 1
            angle = atan2(v, u)
            radius = .86 + .035*sin(phase) + .018*sin(3*phase+.4)
            radius += .018*sin(3*angle+phase) + .012*sin(5*angle-2*phase)
            # Retain useful illumination beyond the room edge (radius .5),
            # rather than spending almost the entire halo on near-zero alpha.
            t = min(1, max(0, (hypot(u, v) - .34) / (radius - .34)))
            falloff = 1 - t*t*(3-2*t)
            pulse = .84 + .10*sin(phase) + .04*sin(3*phase+.4)
            level = round(255 * falloff * pulse)
            sheet.putpixel((frame*SIZE+x,y), (level,level,level,255))
sheet.save(Path(__file__).resolve().parents[1] / 'src/resources/pixmaps/terrain-light-masks.png')
