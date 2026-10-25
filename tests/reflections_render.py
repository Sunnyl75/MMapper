#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
# Copyright (C) 2026 The MMapper Authors
"""Exercise reflection uniforms through the production GL/GLES terrain shader.

Optional arguments after gl/es: a directory exported by TestTerrainMaterialMaps,
and a directory for real-art screenshots. No image generation model is used.
"""
from pathlib import Path
import ctypes as C
import sys
import numpy as np
from PIL import Image

root = Path(__file__).resolve().parent
# Keep its EGL fixture alive for the reflection checks below.
fixture = root/'terrain_edges_render.py'
source = fixture.read_text().split('EGL.eglMakeCurrent(display,EGL.EGL_NO_SURFACE')[0]
h = {'__file__': str(fixture), '__name__': '__main__'}
arguments = sys.argv[:]
sys.argv = arguments[:2]
try:
    exec(compile(source, str(fixture), 'exec'), h)
finally:
    sys.argv = arguments
GL, program, render = h['GL'], h['program'], h['render']

def uniform(name):
    return GL.glGetUniformLocation(program, name)

def settings(mode=1, direction=(.8, 0., .6), strength=1., intensity=1., tint=(1., .86, .65)):
    GL.glUniform4i(uniform('uReflectionAtlas'), 0, 32, mode, 0)
    GL.glUniform4f(uniform('uReflectionLight'), *direction, intensity)
    GL.glUniform4f(uniform('uReflectionColor'), *tint, strength)

def upload(pages):
    GL.glActiveTexture(GL.GL_TEXTURE0)
    GL.glBindTexture(GL.GL_TEXTURE_2D_ARRAY, h['texture'])
    size = pages[0].width
    GL.glTexImage3D(GL.GL_TEXTURE_2D_ARRAY, 0, GL.GL_RGBA8, size, size,
                   len(pages), 0, GL.GL_RGBA, GL.GL_UNSIGNED_BYTE,
                   b''.join(im.tobytes() for im in pages))
    GL.glTexParameteri(GL.GL_TEXTURE_2D_ARRAY, GL.GL_TEXTURE_MIN_FILTER, GL.GL_LINEAR)
    GL.glTexParameteri(GL.GL_TEXTURE_2D_ARRAY, GL.GL_TEXTURE_MAG_FILTER, GL.GL_LINEAR)

colour = Image.new('RGBA', (512, 512), (45, 90, 125, 255))
# A sloping synthetic normal and a stripe mask make rotations and exclusions
# measurable independently of what the current painted artwork looks like.
data = np.empty((512, 512, 4), dtype=np.uint8)
data[:] = (65, 128, 239, 0)
for y in range(512):
    data[y, :, 3] = 210 if 32 < (y % 128) < 96 else 0
normal = Image.fromarray(data)
upload([colour, colour, normal, normal])
checks = 0

def check(condition):
    global checks
    assert condition
    checks += 1

def water(flags=0, tile=0):
    return render(0, raw=(tile, 0, 0, 11 | 4096 | flags))

def pixels(im):
    return np.asarray(im, dtype=float)

def difference(a, b):
    return np.abs(pixels(a)[:, :, :3]-pixels(b)[:, :, :3]).mean()

settings(mode=0)
off = water()
settings()
on = water()
check(difference(off, on) > 3.)
check(np.array_equal(pixels(off)[:, :, 3], pixels(on)[:, :, 3]))
settings(direction=(0., .8, .6))
turned = water()
check(difference(on, turned) > 1.)
settings(intensity=0.)
check(water().tobytes() == off.tobytes())
settings(strength=0.)
check(water().tobytes() == off.tobytes())
settings()
check(water(8192).tobytes() == off.tobytes())  # NO_SKY
settings(mode=0)
land = render(0, raw=(0, 0, 0, 2 | 4096))
settings(mode=2)
check(render(0, raw=(0, 0, 0, 2 | 4096)).tobytes() == land.tobytes())
# Flat normals are explicitly normalized to match mask-only brightness.
flat = Image.new('RGBA', (512, 512), (128, 128, 255, 210))
upload([colour, colour, flat, flat])
settings(mode=1)
mask = water()
settings(mode=2)
check(difference(mask, water()) < .5)
upload([colour, colour, normal, normal])
settings(mode=2)
sloped = water()
check(difference(sloped, on) > .5)
check(difference(sloped, water(256)) > .5)  # Horizontal UV flip rotates normal.
settings(mode=0)
check(water().tobytes() == off.tobytes())

# The shared water undercoat uses the same directional envelope across rooms.
settings(mode=0)
undercoat = render(0, raw=(0, 0, 0, 9 | 4096))
settings(mode=1)
check(difference(undercoat, render(0, raw=(0, 0, 0, 9 | 4096))) > .5)
view = (C.c_float*16)(1,0,0,0, 0,2,0,0, 0,0,1,0, -1,-1,0,1)
render(0, raw=(0,0,0,9 | 4096), pos=(.5,.5), view_override=view)
tiled = render(0, raw=(0,0,0,9 | 4096), pos=(1.5,.5),
               view_override=view, clear=False)
complete = render(0, raw=(0,0,0,9 | 4096), size=2, pos=(1,.5), view_override=view)
check(difference(tiled, complete) < .1)

if len(sys.argv) >= 3:
    art = Path(sys.argv[2])
    pages = [Image.open(art/f'{kind}-{i}.png').convert('RGBA')
             for kind in ['colour', 'data'] for i in range(2)]
    upload(pages)
    samples = []
    for name, direction, tint in [
        ('Dawn', (.98, -.05, .18), (1., .56, .31)),
        ('Midmorning', (.70, -.40, .59), (1., .96, .88)),
        ('Midday', (0., -.60, .80), (1., .96, .88)),
        ('Midafternoon', (-.70, -.40, .59), (1., .96, .88)),
        ('Dusk', (-.98, -.05, .18), (1., .56, .31)),
    ]:
        row = []
        for mode in [0, 1, 2]:
            settings(mode=mode, direction=direction, tint=tint)
            row.append(water(tile=12))
        check(difference(row[0], row[1]) > .1)
        samples.append((name, row))
    if len(sys.argv) >= 4:
        from PIL import ImageDraw, ImageFont
        output = Path(sys.argv[3]); output.mkdir(parents=True, exist_ok=True)
        # Rendered images are flipped for normal on-screen map orientation.
        panel = Image.new('RGB', (3*280+20, 5*292+65), '#20252b')
        draw = ImageDraw.Draw(panel)
        for col, name in enumerate(['Off', 'Reflectivity mask', 'Mask + normals']):
            draw.text((20+280*col, 12), name, fill='white')
        for y, (name, images) in enumerate(samples):
            draw.text((20, 42+y*292), name, fill='white')
            for x, image in enumerate(images):
                background = Image.new('RGBA', image.size, '#344e5d')
                background.alpha_composite(image.transpose(Image.Transpose.FLIP_TOP_BOTTOM))
                panel.paste(background.convert('RGB'), (20+x*280, 62+y*292))
        panel.save(output/'production-water-reflections.png')
print(f'PASS {checks} reflection render assertions')

EGL = h["EGL"]
EGL.eglMakeCurrent(h["display"], EGL.EGL_NO_SURFACE, EGL.EGL_NO_SURFACE, EGL.EGL_NO_CONTEXT)
EGL.eglDestroyContext(h["display"], h["context"])
EGL.eglDestroySurface(h["display"], h["surface"])
EGL.eglTerminate(h["display"])
