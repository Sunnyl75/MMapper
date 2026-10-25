#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
# Copyright (C) 2026 The MMapper Authors
"""Render optional pack details through production shaders (GL or GLES).

Optional third argument is a real terrain/shire art directory.
"""
from pathlib import Path
import sys
import numpy as np
from PIL import Image

fixture=Path(__file__).with_name('reflections_render.py')
state={'__file__':str(fixture),'__name__':'__main__'}
args=sys.argv[:]
sys.argv=args[:2]
try:
    exec(compile(fixture.read_text().split('EGL = h["EGL"]')[0],str(fixture),'exec'),state)
finally:
    sys.argv=args
GL,render,upload=state['GL'],state['render'],state['upload']
uniform,difference=state['uniform'],state['difference']
checks=0

def check(value):
    global checks
    assert value
    checks+=1

art=Path(args[2]) if len(args)>2 else None

def pages(filename,fringe=False):
    result=[Image.new('RGBA',(512,512)) for _ in range(2)]
    if art:
        sheet=Image.open(art/filename).convert('RGBA')
    else:
        sheet=Image.new('RGBA',(384,256),(35,135,45,255) if fringe else (60,90,30,220))
    cell=sheet.width//3
    for slot in range(24):
        role=slot%6
        image=sheet.crop((role%3*cell,role//3*cell,(role%3+1)*cell,(role//3+1)*cell))
        if fringe:
            band=image.getchannel('A').point(lambda a:255 if a>=16 else 0).getbbox()
            if band:image=image.crop((0,band[1],cell,band[3]))
        image=image.resize((128,128),Image.Resampling.LANCZOS).transpose(Image.Transpose.FLIP_TOP_BOTTOM)
        result[slot//16].paste(image,((slot%4)*128,((slot%16)//4)*128))
    return result

base=Image.new('RGBA',(512,512),(125,87,48,255))
tracks=pages('terrain-road-cart-tracks.png')
fringes=pages('terrain-city-wall-fringe.png',True)
wall=Image.new('RGBA',(512,512),(135,90,40,255))
table=np.zeros((512,512,4),dtype=np.uint8)
table[0,:32]=(0,0,0,255);table[1,:32]=(0,0,0,255)
table[1,200]=(32,0,0,255);table[1,201]=(64,0,0,255)
upload([base,base,*tracks,*fringes,wall,wall,Image.fromarray(table)])
GL.glUniform4i(uniform('uReflectionAtlas'),0,0,0,9)
GL.glUniform4f(uniform('uMoonTint'),0,0,0,0)
GL.glUniform4f(uniform('uSkyTint'),0,0,0,0)
GL.glUniform4f(uniform('uRoomLightParams'),0,0,0,0)

def road(mask,pack=1,walls=0):
    return render(0,raw=((pack<<20)|7,0,walls<<24,7|mask<<4))

baseline=road(3,0)
for mask in range(1,16):
    image=road(mask)
    check(image.getchannel('A').getextrema()==(255,255))
    check(difference(image,baseline)>.05)
check(road(3,1,15).tobytes()==baseline.tobytes())
check(road(3,0).tobytes()==baseline.tobytes())
check(difference(road(3),road(12))>1.)
# Boundary strips are canonical even when interior roles differ.
a=np.asarray(road(3));b=np.asarray(road(5));c=np.asarray(road(7));d=np.asarray(road(15))
check(np.abs(a[-1].astype(float)-b[-1].astype(float)).mean()<.1)
check(np.abs(a[-1].astype(float)-c[-1].astype(float)).mean()<.1)
check(np.abs(a[-1].astype(float)-d[-1].astype(float)).mean()<.1)
check(np.abs(a[0].astype(float)-a[-1].astype(float)).mean()<.1)
# The extra pack bits preserve explicit wall tile indices in every orientation.
for turn in range(4):
    flags=12|4096|16384|(28<<20)|(turn<<4)
    ordinary=render(0,raw=(96,0,0,flags))
    grass=render(0,raw=((1<<20)|96,0,0,flags))
    check(difference(ordinary,grass)>.2)
    check(np.asarray(grass)[:,:,3].sum()>np.asarray(ordinary)[:,:,3].sum())
check(GL.glGetError()==GL.GL_NO_ERROR)
print(f'PASS {checks} optional pack detail render assertions'+(' with real Shire artwork' if art else ''))
