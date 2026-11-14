#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
# Copyright (C) 2026 The MMapper Authors
"""Test all material profiles through production GL/GLES shaders.

Usage: python reflections_materials_render.py gl|es [native-map-export] [preview-output]
The optional export comes from TestTerrainMaterialMaps <pixmaps-dir> <export-dir>.
"""
from pathlib import Path
import ctypes as C
import sys
import numpy as np
from PIL import Image, ImageDraw

base = Path(__file__).with_name('reflections_render.py')
state = {'__file__': str(base), '__name__': '__main__'}
arguments = sys.argv[:]
sys.argv = arguments[:2]
try:
    exec(compile(base.read_text().split('EGL = h["EGL"]')[0], str(base), 'exec'), state)
finally:
    sys.argv = arguments
GL, render, upload = state['GL'], state['render'], state['upload']
uniform, difference = state['uniform'], state['difference']
checks = 0

def check(value):
    global checks
    assert value
    checks += 1

def settings(mode=2, direction=(.8, -.2, .56), strength=1.5, tint=(1., .86, .65)):
    GL.glUniform4i(uniform('uReflectionAtlas'), 0, 128, mode, 0)
    GL.glUniform4i(uniform('uReflectionMaterials'), 32, 64, 96, -1)
    GL.glUniform4f(uniform('uReflectionLight'), *direction, 1.)
    GL.glUniform4f(uniform('uReflectionColor'), *tint, strength)

def draw(profile, flags=0, variant=0, **kwargs):
    kind = 2 if profile == 1 else 8 if profile == 3 else 11
    if profile == 3:
        kwargs['pos'] = (.5, .06)
        flags |= 16384
    elif profile == 2:
        flags |= 524288  # The native fixed rapids-overlay flag.
    return render(0, raw=(profile*32+variant, 0, 0, kind | 4096 | flags), **kwargs)

colour = Image.new('RGBA', (512,512), (65,100,130,255))
normal = Image.new('RGBA', (512,512), (70,128,240,180))
upload([colour]*8+[normal]*8)
for profile in [1,2,3]:
    settings(mode=0)
    off = draw(profile)
    settings()
    on = draw(profile)
    check(difference(off,on) > .5)
    check(np.array_equal(np.asarray(off)[:,:,3],np.asarray(on)[:,:,3]))
    check(draw(profile,8192).tobytes() == off.tobytes())
    settings(strength=0.)
    check(draw(profile).tobytes() == off.tobytes())
    settings(mode=0)
    shadow_off = draw(profile,shadow=True)
    settings()
    check(draw(profile,shadow=True).tobytes() == shadow_off.tobytes())
# Geometry rotations must rotate a marsh normal into world coordinates.
settings(direction=(.8,0.,.6))
original = draw(1)
settings(direction=(0.,.8,.6))
quarter = draw(1,1<<4)
check(difference(original.transpose(Image.Transpose.ROTATE_90),quarter) < .2)
# Mountain glints use the upright facing, not a ground-centred stripe.
settings(mode=0)
mountain = draw(3)
settings(direction=(0.,.995,.1))
check(difference(mountain,draw(3)) < .1)
settings(direction=(0.,-.995,.1))
check(difference(mountain,draw(3)) > .5)

if len(arguments) >= 3:
    folder = Path(arguments[2])
    names = ['water','shallows','rapids','mountains']
    colours = [Image.open(folder/name/f'colour-{page}.png').convert('RGBA')
               for name in names for page in range(2)]
    maps = [Image.open(folder/name/f'data-{page}.png').convert('RGBA')
            for name in names for page in range(2)]
    upload(colours+maps)
    # All actual seasonal water/marsh/mountain slots, plus all fixed rapids.
    for profile in range(4):
        visible = 0
        for variant in range(24):
            settings(mode=0)
            off = draw(profile,variant=variant)
            settings(direction=(.45,-.70,.55))
            on = draw(profile,variant=variant)
            check(np.array_equal(np.asarray(off)[:,:,3],np.asarray(on)[:,:,3]))
            visible += difference(off,on) > .02
        check(visible >= 18)  # Some winter marsh pools can be entirely frozen.
    if len(arguments) >= 4:
        output = Path(arguments[3]); output.mkdir(parents=True,exist_ok=True)
        panel = Image.new('RGB',(1140,4*280+60),'#242b31')
        canvas = ImageDraw.Draw(panel)
        for i,label in enumerate(['Material','Off','Morning / normals','Afternoon / normals']):
            canvas.text((10+280*i,10),label,fill='white')
        mask_panel = panel.copy(); masks_draw=ImageDraw.Draw(mask_panel)
        masks_draw.rectangle((0,0,1140,50),fill='#242b31')
        for i,label in enumerate(['Material','Original','Reflectivity selection','Normals (data RGB)']):
            masks_draw.text((10+280*i,10),label,fill='white')
        for profile,name in enumerate(names):
            variant = 2 if profile==2 else 0 if profile==3 else 12
            label=name+' (winter)' if profile==3 else name
            canvas.text((10,70+280*profile),label,fill='white')
            masks_draw.text((10,70+280*profile),label,fill='white')
            images=[]
            settings(mode=0);images.append(draw(profile,variant=variant))
            settings(direction=(.70,-.40,.59));images.append(draw(profile,variant=variant))
            settings(direction=(-.70,-.40,.59));images.append(draw(profile,variant=variant))
            for col,im in enumerate(images):
                back=Image.new('RGBA',im.size,'#34444d')
                back.alpha_composite(im.transpose(Image.Transpose.FLIP_TOP_BOTTOM))
                panel.paste(back.convert('RGB'),(290+col*280,50+profile*280))
            page,cell=variant//16,variant%16
            box=(cell%4*128,cell//4*128,(cell%4+1)*128,(cell//4+1)*128)
            art=colours[profile*2+page].crop(box).transpose(Image.Transpose.FLIP_TOP_BOTTOM)
            data=maps[profile*2+page].crop(box).transpose(Image.Transpose.FLIP_TOP_BOTTOM)
            selection=Image.fromarray(np.repeat(np.asarray(data)[:,:,3:4],3,axis=2)).convert('RGBA')
            normals=data.convert('RGB').convert('RGBA')
            for col,im in enumerate([art,selection,normals]):
                back=Image.new('RGBA',im.size,'#34444d');back.alpha_composite(im)
                mask_panel.paste(back.convert('RGB').resize((256,256)),(290+col*280,50+profile*280))
        panel.save(output/'terrain-reflections-comparison.png')
        mask_panel.save(output/'terrain-reflection-masks.png')
print(f'PASS {checks} additional material render assertions')
fixture=state['h'];EGL=fixture['EGL']
EGL.eglMakeCurrent(fixture['display'],EGL.EGL_NO_SURFACE,EGL.EGL_NO_SURFACE,EGL.EGL_NO_CONTEXT)
EGL.eglDestroyContext(fixture['display'],fixture['context'])
EGL.eglDestroySurface(fixture['display'],fixture['surface'])
EGL.eglTerminate(fixture['display'])
