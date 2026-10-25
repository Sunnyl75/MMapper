#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
# Copyright (C) 2026 The MMapper Authors
"""Exercise regional indirection using the production OpenGL/GLES shaders."""
from pathlib import Path
import sys
import numpy as np
from PIL import Image

fixture = Path(__file__).with_name('reflections_render.py')
state = {'__file__': str(fixture), '__name__': '__main__'}
source = fixture.read_text().split('EGL = h["EGL"]')[0]
arguments = sys.argv[:]
sys.argv = arguments[:2]
try:
    exec(compile(source, str(fixture), 'exec'), state)
finally:
    sys.argv = arguments
GL, render, upload = state['GL'], state['render'], state['upload']
uniform, difference = state['uniform'], state['difference']
checks = 0

def check(value):
    global checks
    assert value
    checks += 1

normal = Image.new('RGBA', (512, 512), (128, 128, 255, 210))
default = Image.new('RGBA', (512, 512), (110, 65, 35, 255))
regional = Image.new('RGBA', (512, 512), (35, 130, 75, 255))
table = np.zeros((512, 512, 4), dtype=np.uint8)
table[0, :32] = (0, 0, 0, 255)
table[1, :32] = (32, 0, 0, 255)
# Keep the road shared while replacing the forest/field family. This catches
# accidental assumptions that all regional families occupy consecutive pages.
table[1, 11] = (0, 0, 0, 255)
table[64, 2:4] = (64, 0, 1, 255)  # Regional water has its own data pages.

def settings(mode=0):
    GL.glUniform4i(uniform('uReflectionAtlas'), 0, 64, mode, 7)
    GL.glUniform4f(uniform('uReflectionLight'), .8, 0., .6, 1.)
    GL.glUniform4f(uniform('uReflectionColor'), 1., .86, .65, 1.5)

upload([default, default, regional, regional, normal, normal, Image.fromarray(table)])
settings()
baseline = render(0, raw=(2, 0, 0, 3))
local = render(0, raw=((1 << 20) | 2, 0, 0, 3))
check(difference(baseline, local) > 25.)
check(local.getpixel((128, 128))[:3] == (35, 130, 75))
check(render(0, raw=((1 << 20) | 7, 0, 0, 7)).getpixel((128, 128))[:3]
      == (110, 65, 35))  # Road uses the inherited family, not the new forest.
check(render(0, raw=(32, 0, 0, 15 | 4096)).getpixel((128, 128))[:3]
      == (35, 130, 75))  # Repeat paving has the explicit regional layer.

off = render(0, raw=(32, 0, 0, 11 | 4096))
settings(2)
on = render(0, raw=(32, 0, 0, 11 | 4096))
check(difference(off, on) > 1.)
check(render(0, raw=(32, 0, 0, 11 | 4096 | 8192)).tobytes() == off.tobytes())
# Default colour was deliberately not assigned a reflection profile.
settings(0)
unreflective = render(0, raw=(0, 0, 0, 11 | 4096))
settings(2)
check(render(0, raw=(0, 0, 0, 11 | 4096)).tobytes() == unreflective.tobytes())

# Regional ground fill can remain green in winter even where the art is
# translucent. No change to the clock or global seasonal palette is required.
transparent = Image.new('RGBA', (512, 512), (35, 130, 75, 0))
table[1, 32:36] = (35, 100, 50, 255)  # Forest family 0's four seasonal fills.
upload([default, default, transparent, transparent, normal, normal, Image.fromarray(table)])
settings()
winter = render(0, raw=((1 << 20) | 2, 0, 0, 3), day=1)
check(winter.getpixel((128, 128))[:3] == (35, 100, 50))
check(GL.glGetError() == GL.GL_NO_ERROR)
print(f'PASS {checks} regional pack render assertions')
