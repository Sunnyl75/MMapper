#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
# Copyright (C) 2026 The MMapper Authors
"""Render optional pack details through production shaders (GL or GLES).

Optional third argument is a real terrain/shire art directory.
"""
from pathlib import Path
import sys
import numpy as np
from PIL import Image, ImageDraw

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
    if art and (art/filename).exists():
        sheet=Image.open(art/filename).convert('RGBA')
    else:
        sheet=Image.new('RGBA',(384,256))
        draw=ImageDraw.Draw(sheet)
        for role in range(6):
            x=(role%3)*128;y=(role//3)*128
            draw.ellipse((x+25,y+35,x+103,y+93),fill=(35+role*5,130,40,220))
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
tracks=pages('terrain-road-centre-grass.png')
fringes=pages('terrain-city-wall-fringe.png',True)
wall=Image.new('RGBA',(512,512),(135,90,40,255))
table=np.zeros((512,512,4),dtype=np.uint8)
table[0,:32]=(0,0,0,255);table[1,:32]=(0,0,0,255)
table[1,200]=(32,0,0,255);table[1,201]=(64,0,0,255)
field=Image.new('RGBA',(512,512),(35,110,45,255))
table[1,1]=(128,0,0,255)
upload([base,base,*tracks,*fringes,wall,wall,field,field,Image.fromarray(table)])
GL.glUniform4i(uniform('uReflectionAtlas'),0,0,0,11)
GL.glUniform4f(uniform('uMoonTint'),0,0,0,0)
GL.glUniform4f(uniform('uSkyTint'),0,0,0,0)
GL.glUniform4f(uniform('uRoomLightParams'),0,0,0,0)

def road(mask,pack=1,walls=0,seed=0):
    return render(0,raw=((pack<<20)|7,0,(seed & 65535) | walls<<24,7|mask<<4))

baseline=road(3,0)
for mask in range(1,16):
    image=road(mask)
    check(image.getchannel('A').getextrema()==(255,255))
    check(difference(image,baseline)>.05)
check(road(3,1,15).tobytes()==baseline.tobytes())
check(road(3,0).tobytes()==baseline.tobytes())
check(difference(road(3),road(12))>1.)
# An open city connection adds the missing end of a road. The resulting
# dirt lanes reach the paving boundary; the former dead-end is grass there.
for side in range(4):
    opposite=side^1
    city_sectors=7|(8<<((side+1)*4))
    connected=np.asarray(render(0,raw=((1<<20)|city_sectors,0,0,7|((1<<side)|(1<<opposite))<<4))).astype(float)
    capped=np.asarray(render(0,raw=((1<<20)|city_sectors,0,0,7|(1<<opposite)<<4))).astype(float)
    if side<2:
        end=-1 if side==0 else 0
        check(np.abs(connected[end,52:58,:3]-capped[end,52:58,:3]).mean()>2.0)
        check(np.abs(connected[end,198:204,:3]-capped[end,198:204,:3]).mean()>2.0)
    else:
        end=-1 if side==2 else 0
        check(np.abs(connected[52:58,end,:3]-capped[52:58,end,:3]).mean()>2.0)
        check(np.abs(connected[198:204,end,:3]-capped[198:204,end,:3]).mean()>2.0)
# A seed belongs to the room, not to the foreground stamp lattice.
for mask in range(1,16):
    check(road(mask,seed=1).tobytes()==road(mask,seed=65534).tobytes())
# Connected boundaries compare actual adjacent world coordinates, not copied
# room-local tiles. Tiny natural texture gradients are allowed; cracks are not.
import ctypes as C
def placed(mask,x,y):
    view=(C.c_float*16)(2,0,0,0,0,2,0,0,0,0,1,0,-1-2*x,-1-2*y,0,1)
    return np.asarray(render(0,raw=((1<<20)|7,0,997*(x+y*3),7|mask<<4),
                             pos=(x+.5,y+.5),view_override=view)).astype(float)
for left in range(1,16):
    for right in range(1,16):
        if left & 4 and right & 8:
            a=placed(left,0,0);b=placed(right,1,0)
            check(np.abs(a[:,-1,:3]-b[:,0,:3]).mean()<3.0)
        if left & 1 and right & 2:
            a=placed(left,0,0);b=placed(right,0,1)
            check(np.abs(a[-1,:,:3]-b[0,:,:3]).mean()<3.0)
# The middle lane contains tufts, while the two wheel-worn lanes remain dirt.
image=np.asarray(road(3)).astype(float);empty=np.asarray(road(0)).astype(float)
check(np.abs(image[:,95:161,:3]-empty[:,95:161,:3]).mean()>0.2)
check(np.abs(image[:,52:58,:3]-empty[:,52:58,:3]).max()==0)
check(np.abs(image[:,198:204,:3]-empty[:,198:204,:3]).max()==0)
# One populated centre band: each twelfth of a straight has foreground,
# with more coverage at the centre than in either worn dirt lane. Test both
# orientations so a two-row lattice cannot return after a 90-degree turn.
for mask in (3,12):
    lane=np.abs(np.asarray(road(mask)).astype(float)-empty)[:,:,:3].max(axis=2)
    if mask==12:lane=lane.T
    centre=lane[:,116:140].mean()
    check(centre>lane[:,52:58].mean()+1.0)
    check(centre>lane[:,198:204].mean()+1.0)
    check((lane[:,90:166]>2.0).mean()>0.65)
    check(lane[:,:16].mean()>1.0 and lane[:,240:].mean()>1.0)
    for station in range(12):
        part=lane[station*256//12:(station+1)*256//12,108:148]
        check(part.max()>2.0)
# Panning by an exact output pixel preserves the visible world-space stamps.
raw=((1<<20)|7,0,0,7|3<<4)
view=(C.c_float*16)(2,0,0,0,0,2,0,0,0,0,1,0,-1+2/256,-1,0,1)
panned=np.asarray(render(0,raw=raw,view_override=view)).astype(float)
check(np.abs(image[:,:-1,:3]-panned[:,1:,:3]).mean()<0.1)
# Replacing only the field palette must recolour the grass, while earth
# outside the centre remains byte-identical. This detects baked yellow hues.
blue=Image.new('RGBA',(512,512),(30,65,150,255))
upload([base,base,*tracks,*fringes,wall,wall,blue,blue,Image.fromarray(table)])
blue_lane=np.asarray(road(3)).astype(float)
check(np.abs(blue_lane[:,95:161,:3]-image[:,95:161,:3]).mean()>2.0)
check(np.array_equal(blue_lane[:,52:58],image[:,52:58]))
check(np.array_equal(blue_lane[:,198:204],image[:,198:204]))
upload([base,base,*tracks,*fringes,wall,wall,field,field,Image.fromarray(table)])
# Full winter uses the snowy road material without green foreground.
winter=render(0,raw=raw,day=30)
check(winter.tobytes()==render(0,raw=(raw[0],0,0,7),day=30).tobytes())
# Shared street/road material: both room owners reach the same mix at their
# boundary, while indoors retains its original paving despite pack metadata.
city=Image.new('RGBA',(512,512),(170,150,115,255))
red_border=Image.new('RGBA',(512,512),(220,25,25,255))
join_table=table.copy();join_table[1,19]=(160,0,0,255);join_table[1,13]=(192,0,0,255)
def join_upload(border):
    upload([base,base,*tracks,*fringes,wall,wall,field,field,city,city,border,border,Image.fromarray(join_table)])
    GL.glUniform4i(uniform('uReflectionAtlas'),0,0,0,15)
join_upload(red_border)
plain_city=render(0,raw=((1<<20)|160,0,0,15|4096))
warm_city=render(0,raw=((1<<20)|160,0,0,15|4096|32768))
check(difference(plain_city,warm_city)>0.5)
check(plain_city.tobytes()==render(0,raw=(160,0,0,15|4096)).tobytes())
for side in range(4):
    x=1 if side==2 else -1 if side==3 else 0
    y=1 if side==0 else -1 if side==1 else 0
    view=(C.c_float*16)(2,0,0,0,0,2,0,0,0,0,1,0,-1-2*x,-1-2*y,0,1)
    city_join=np.asarray(render(0,raw=((1<<20)|160,0,0,15|4096|32768|(1<<(side^1))<<4),
                                pos=(x+.5,y+.5),view_override=view)).astype(float)
    road_join=np.asarray(render(0,raw=((1<<20)|7|(8<<((side+1)*4)),0,0,7|(1<<side)<<4))).astype(float)
    if side<2:
        edge=-1 if side==0 else 0;peer=0 if side==0 else -1
        check(np.abs(road_join[edge,:,:3]-city_join[peer,:,:3]).mean()<2.0)
    else:
        edge=-1 if side==2 else 0;peer=0 if side==2 else -1
        check(np.abs(road_join[:,edge,:3]-city_join[:,peer,:3]).mean()<2.0)
    blocked=render(0,raw=((1<<20)|160,0,(1<<(side^1))<<24,15|4096|32768|(1<<(side^1))<<4))
    check(blocked.tobytes()==warm_city.tobytes())
# Decorative border artwork must never affect an apron. Saturated alternate
# border colours make even a one-pixel curved fringe measurable.
aprons=[render(0,raw=((1<<20)|7,0,0,5|32768|mask<<4)) for mask in range(1,16)]
trail=render(0,raw=((1<<20)|1,0,0,6|3<<4))
join_upload(Image.new('RGBA',(512,512),(25,25,220,255)))
for mask,expected in enumerate(aprons,1):
    check(render(0,raw=((1<<20)|7,0,0,5|32768|mask<<4)).tobytes()==expected.tobytes())
check(difference(trail,render(0,raw=((1<<20)|1,0,0,6|3<<4)))>0.1)
upload([base,base,*tracks,*fringes,wall,wall,field,field,Image.fromarray(table)])
GL.glUniform4i(uniform('uReflectionAtlas'),0,0,0,11)
# The extra pack bits preserve explicit wall tile indices in every orientation.
for turn in range(4):
    flags=12|4096|16384|(28<<20)|(turn<<4)
    ordinary=render(0,raw=(96,0,0,flags))
    grass=render(0,raw=((1<<20)|96,0,0,flags))
    check(difference(ordinary,grass)>.2)
    check(np.asarray(grass)[:,:,3].sum()>np.asarray(ordinary)[:,:,3].sum())
# Garden artwork is rendered on full square sprite quads, independently of
# wall clipping. Ninety-degree turns preserve the complete alpha silhouette.
gardens=pages('terrain-city-wall-gardens.png')
garden_table=table.copy();garden_table[1,201]=(0,0,0,0);garden_table[1,202]=(160,0,0,255)
upload([base,base,*tracks,*fringes,wall,wall,field,field,*gardens,Image.fromarray(garden_table)])
GL.glUniform4i(uniform('uReflectionAtlas'),0,0,0,13)
garden_flags=10|4096|8192|1048576
for variant in range(6):
    original=np.asarray(render(0,raw=(160+variant,0,0,garden_flags),size=.4))
    for turn in range(4):
        rotated=np.asarray(render(0,raw=(160+variant,0,15<<24,garden_flags|turn<<4),size=.4))
        expected=np.rot90(original,-turn)
        check(np.abs(rotated.astype(float)-expected.astype(float)).mean()<0.1)
    centre=render(0,raw=(160+variant,0,0,garden_flags),size=.2)
    for x,y in ((.12,.12),(.88,.12),(.12,.88),(.88,.88)):
        corner=render(0,raw=(160+variant,0,15<<24,garden_flags),size=.2,pos=(x,y))
        check(abs(np.asarray(corner)[:,:,3].sum()/np.asarray(centre)[:,:,3].sum()-1)<.03)
# Garden metadata no longer widens or decorates the wall shader's gutter.
for turn in range(4):
    flags=12|4096|16384|(28<<20)|(turn<<4)
    plain=render(0,raw=(96,0,0,flags))
    garden_wall=render(0,raw=((1<<20)|96,0,0,flags))
    check(np.asarray(garden_wall)[:,:,3].sum()<=np.asarray(plain)[:,:,3].sum())
check(GL.glGetError()==GL.GL_NO_ERROR)
print(f'PASS {checks} optional pack detail render assertions'+(' with real Shire artwork' if art else ''))
