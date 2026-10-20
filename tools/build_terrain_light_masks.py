#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""Rebuild the small, seamless-loop light atlas. Pillow is needed only to regenerate it."""
from math import cos, exp, pi, sin
from pathlib import Path
from PIL import Image

SIZE, FRAMES = 32, 16
sheet = Image.new('RGBA', (SIZE * FRAMES, SIZE))
for frame in range(FRAMES):
    phase = frame * 2 * pi / FRAMES
    for y in range(SIZE):
        for x in range(SIZE):
            u, v = x / (SIZE - 1), y / (SIZE - 1)
            edge = min(u, v, 1-u, 1-v)
            t = min(1, max(0, edge / .12))
            boundary = t * t * (3 - 2 * t)
            value = .94 + .06*sin(phase) + .025*sin(3*phase+.4)
            for corner, (cx, cy) in enumerate(((0,0), (1,0), (1,1), (0,1))):
                angle = phase + corner * 1.73
                width = .13 + .065*(1 + sin(angle))
                sx = cx + .10*sin(angle*1 if corner%2 else angle+1)
                sy = cy + .10*cos(angle)
                shadow = exp(-((u-sx)**2+(v-sy)**2)/(2*width**2))
                value -= (.30 + .23*(1+cos(angle))) * shadow
            level = round(255 * boundary * min(1, max(.12, value)))
            sheet.putpixel((frame*SIZE+x,y), (level,level,level,255))
sheet.save(Path(__file__).resolve().parents[1] / 'src/resources/pixmaps/terrain-light-masks.png')
