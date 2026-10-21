#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
# Copyright (C) 2026 The MMapper Authors
"""Render the production terrain shaders using surfaceless Mesa GL or GLES."""
import os
os.environ['PYOPENGL_PLATFORM'] = 'egl'
from pathlib import Path
import ctypes as C
import sys
from OpenGL import EGL, GL
from OpenGL.EGL.EXT.platform_base import eglGetPlatformDisplayEXT
from PIL import Image

api = sys.argv[1] if len(sys.argv) > 1 else 'gl'
display = eglGetPlatformDisplayEXT(0x31DD, EGL.EGL_DEFAULT_DISPLAY, None)
major, minor = EGL.EGLint(), EGL.EGLint()
assert EGL.eglInitialize(display, major, minor)
es = api == 'es'
assert EGL.eglBindAPI(EGL.EGL_OPENGL_ES_API if es else EGL.EGL_OPENGL_API)
attributes = (EGL.EGLint * 13)(EGL.EGL_SURFACE_TYPE, EGL.EGL_PBUFFER_BIT,
    EGL.EGL_RENDERABLE_TYPE, 0x0040 if es else EGL.EGL_OPENGL_BIT,
    EGL.EGL_RED_SIZE, 8, EGL.EGL_GREEN_SIZE, 8, EGL.EGL_BLUE_SIZE, 8,
    EGL.EGL_ALPHA_SIZE, 8, EGL.EGL_NONE)
configs = (EGL.EGLConfig * 1)()
count = EGL.EGLint()
assert EGL.eglChooseConfig(display, attributes, configs, 1, count) and count.value
surface = EGL.eglCreatePbufferSurface(display, configs[0],
    (EGL.EGLint * 5)(EGL.EGL_WIDTH, 256, EGL.EGL_HEIGHT, 256, EGL.EGL_NONE))
context_attributes = ((EGL.EGLint * 3)(EGL.EGL_CONTEXT_CLIENT_VERSION, 3, EGL.EGL_NONE) if es
    else (EGL.EGLint * 7)(0x3098, 3, 0x30FB, 3, 0x30FD, 1, EGL.EGL_NONE))
context = EGL.eglCreateContext(display, configs[0], EGL.EGL_NO_CONTEXT, context_attributes)
assert EGL.eglMakeCurrent(display, surface, surface, context)
print(api, GL.glGetString(GL.GL_VERSION).decode())
root = Path(__file__).resolve().parents[1]
header = '#version 300 es\nprecision highp float;\nprecision mediump sampler2DArray;\n' if es else '#version 330\n'
header += '#define MAX_NAMED_COLORS 256\n'
program = GL.glCreateProgram()
for stage, kind in [('vert', GL.GL_VERTEX_SHADER), ('frag', GL.GL_FRAGMENT_SHADER)]:
    shader = GL.glCreateShader(kind)
    source = (root / f'src/resources/shaders/legacy/terrain/{stage}.glsl').read_text()
    if stage == 'frag': source = (root/'src/resources/shaders/legacy/room-lighting.glsl').read_text()+'\n'+source
    GL.glShaderSource(shader, header + source)
    GL.glCompileShader(shader)
    assert GL.glGetShaderiv(shader, GL.GL_COMPILE_STATUS), GL.glGetShaderInfoLog(shader)
    GL.glAttachShader(program, shader)
GL.glLinkProgram(program)
assert GL.glGetProgramiv(program, GL.GL_LINK_STATUS), GL.glGetProgramInfoLog(program)
GL.glUseProgram(program)
GL.glUniform1i(GL.glGetUniformLocation(program,'uRoomLights'),1)
GL.glBindVertexArray(GL.glGenVertexArrays(1))
ubo = GL.glGenBuffers(1)
GL.glBindBuffer(GL.GL_UNIFORM_BUFFER, ubo)
white = (C.c_float * 1024)(*([1.] * 1024))
GL.glBufferData(GL.GL_UNIFORM_BUFFER, C.sizeof(white), white, GL.GL_STATIC_DRAW)
block = GL.glGetUniformBlockIndex(program, 'NamedColorsBlock')
GL.glUniformBlockBinding(program, block, 0)
GL.glBindBufferBase(GL.GL_UNIFORM_BUFFER, 0, ubo)
matrix = (C.c_float * 16)(2,0,0,0, 0,2,0,0, 0,0,1,0, -1,-1,0,1)
GL.glUniformMatrix4fv(GL.glGetUniformLocation(program, 'uMVP'), 1, False, matrix)
GL.glUniform4f(GL.glGetUniformLocation(program, 'uColor'), 1,1,1,1)
GL.glUniform4f(GL.glGetUniformLocation(program, 'uTerrainShadow'), 0,0,0,0)
# Opaque synthetic atlas materials isolate route geometry from artwork. The
# real bundled atlases are exercised by the Qt app capture. Border family is
# transparent here, so verge decoration cannot obscure endpoint assertions.
texture = GL.glGenTextures(1)
GL.glBindTexture(GL.GL_TEXTURE_2D_ARRAY, texture)
# Real authored shoreline silhouettes; synthetic colours for other materials.
# Match the Qt loader's padded alpha-bounds framing, at 32 pixels per cell.
source_shores = Image.open(root/'src/resources/pixmaps/terrain-road-shores.png').convert('RGBA')
shore_frames=[]
for slot in range(24):
    source_cell=source_shores.width//6
    frame=source_shores.crop((slot%6*source_cell,slot//6*source_cell,
                             (slot%6+1)*source_cell,(slot//6+1)*source_cell))
    bounds=frame.getchannel('A').point(lambda a: 255 if a>=32 else 0).getbbox()
    fitted=Image.new('RGBA',(256,256))
    if bounds: fitted.paste(frame.crop(bounds).resize((252,252),Image.Resampling.BILINEAR),(2,2))
    shore_frames.append(fitted.resize((32,32),Image.Resampling.BILINEAR).transpose(Image.Transpose.FLIP_TOP_BOTTOM))
materials=bytearray()
for family in range(30):
    for page in range(2):
        atlas=Image.new('RGBA',(128,128))
        for cell in range(16):
            slot=page*16+cell
            season=(slot//6)%4
            rgb=(220,225,230) if season==0 else ((40+family*13)%256,95,55)
            tile=shore_frames[slot] if family==29 and slot<24 else Image.new('RGBA',(32,32),(*rgb,0 if family in [13,29] else 255))
            atlas.paste(tile,((cell%4)*32,(cell//4)*32))
        materials.extend(atlas.tobytes())
GL.glTexImage3D(GL.GL_TEXTURE_2D_ARRAY,0,GL.GL_RGBA8,128,128,60,0,GL.GL_RGBA,GL.GL_UNSIGNED_BYTE,bytes(materials))
GL.glTexParameteri(GL.GL_TEXTURE_2D_ARRAY,GL.GL_TEXTURE_MIN_FILTER,GL.GL_NEAREST)
GL.glTexParameteri(GL.GL_TEXTURE_2D_ARRAY,GL.GL_TEXTURE_MAG_FILTER,GL.GL_NEAREST)
GL.glUniform1i(GL.glGetUniformLocation(program,'uTexture'),0)
vbo = GL.glGenBuffers(1)
GL.glBindBuffer(GL.GL_ARRAY_BUFFER, vbo)
GL.glEnableVertexAttribArray(0)
GL.glVertexAttribPointer(0, 4, GL.GL_FLOAT, False, 32, C.c_void_p(0))
GL.glVertexAttribDivisor(0, 1)
GL.glEnableVertexAttribArray(1)
GL.glVertexAttribIPointer(1, 4, GL.GL_INT, 32, C.c_void_p(16))
GL.glVertexAttribDivisor(1, 1)
GL.glViewport(0,0,256,256)
class Instance(C.Structure):
    _fields_ = [('pos', C.c_float * 4), ('data', C.c_int * 4)]

def render(kind, here=7, sides=(1,1,1,1), mask=3, seed=12345, day=180, size=1, neighbours=0, wide=False, shade=1, walls=0, shadow=False, raw=None, sky=(.2,.8), light=0.0, reuse=False, view_override=None):
    packed = here | sum(s << (4 * (i + 1)) for i, s in enumerate(sides))
    instance = Instance((C.c_float * 4)(.5,.5,size,0),
                        (C.c_int * 4)(packed,0,seed | neighbours << 16 | walls << 24,kind | mask << 4))
    if raw is not None:
        instance.data = (C.c_int * 4)(*raw)
    if not reuse:
        GL.glBufferData(GL.GL_ARRAY_BUFFER, C.sizeof(instance), C.byref(instance), GL.GL_STATIC_DRAW)
    view = (C.c_float * 16)(2/3,0,0,0, 0,2/3,0,0, 0,0,1,0, -1/3,-1/3,0,1) if wide else matrix
    if view_override is not None: view = view_override
    GL.glUniformMatrix4fv(GL.glGetUniformLocation(program, 'uMVP'),1,False,view)
    GL.glUniform4f(GL.glGetUniformLocation(program, 'uTerrainShadow'),*sky,.3 if shadow else light,1 if shadow else 0)
    GL.glUniform4f(GL.glGetUniformLocation(program, 'uColor'),shade,shade,shade,1)
    GL.glUniform1f(GL.glGetUniformLocation(program, 'uTerrainDay'), day)
    GL.glClearColor(0,0,0,0)
    GL.glClear(GL.GL_COLOR_BUFFER_BIT)
    GL.glDrawArraysInstanced(GL.GL_TRIANGLE_STRIP, 0, 4, 1)
    GL.glFinish()
    assert GL.glGetError() == GL.GL_NO_ERROR
    data = GL.glReadPixels(0,0,256,256,GL.GL_RGBA,GL.GL_UNSIGNED_BYTE)
    return Image.frombytes('RGBA',(256,256),bytes(data))

def alpha(im): return im.getchannel('A')
checks = 0
def check(condition):
    global checks
    assert condition
    checks += 1

for sides in [(0,0,0,0),(6,6,6,6),(0,6,0,6)]:
    check(alpha(render(4,here=6,sides=sides)).getextrema() == (0,0))
points = [(128,255),(128,0),(255,128),(0,128)]
for sector in [1,2,3,4,5,7,8,9,10]:
    for direction in range(4):
        sides = [6] * 4
        sides[direction] = sector
        im = render(4,here=6,sides=sides)
        check(im.getpixel(points[direction])[3] > 240)
        check(im.getpixel((128,128))[3] == 0)
        for other in range(4):
            if other != direction: check(im.getpixel(points[other])[3] == 0)

grass = render(4,here=6,sides=(1,6,6,6)).getpixel((128,255))
rock = render(4,here=6,sides=(4,6,6,6)).getpixel((128,255))
check(sum(abs(grass[i]-rock[i]) for i in range(3)) > 25)
for kind in [5,6]:
    for mask in range(1,16):
        im = render(kind,here=7 if kind == 5 else 1,mask=mask)
        check(im.getpixel((128,128))[3] > 240)
        for direction in range(4):
            check((im.getpixel(points[direction])[3] > 240) == bool(mask & (1 << direction)))
        for corner in [(0,0),(0,255),(255,0),(255,255)]:
            check(im.getpixel(corner)[3] == 0)

# Roads should read as broad roads, with trails retaining their narrow width.
road_width = sum(a > 240 for a in alpha(render(5, sides=(7,7,1,1), mask=3)).crop((0,255,256,256)).tobytes())
trail_width = sum(a > 240 for a in alpha(render(6, here=1, sides=(1,1,1,1), mask=3)).crop((0,255,256,256)).tobytes())
check(road_width > trail_width * 3)

for mask in range(16):
    for walls in [0,1,2,4,8,15]:
        filled=render(7,sides=(1,2,6,0),mask=mask,walls=walls)
        check(alpha(filled).getextrema() == (255,255))
        check(filled.getpixel((128,128))[:3] == filled.getpixel((0,0))[:3])
# Authored shoreline strips use rounded overlapping ends, with matched road
# colour and no invented edge across a wall. All 24 fixed silhouettes are used.
shore_kind=4 | 32768
shore_images=[]
for direction in range(4):
    sides=[6]*4
    sides[direction]=7
    for variant in range(24):
        im=render(shore_kind,here=6,sides=sides,mask=1<<direction,seed=variant,neighbours=255)
        check(im.getpixel(points[direction])[3]>240)
        check(im.getpixel((128,128))[3]==0)
        check(im.tobytes()==render(shore_kind,here=6,sides=sides,mask=1<<direction,
                                   seed=variant,neighbours=255).tobytes())
        # Opaque road-facing pixels have exactly the underlying road colour.
        check(im.getpixel(points[direction])[:3]==render(7).getpixel((128,128))[:3])
        check(alpha(render(shore_kind,here=6,sides=sides,mask=1<<direction,
                           seed=variant,walls=1<<direction)).getextrema()==(0,0))
        if direction==0: shore_images.append(im.tobytes())
    check(alpha(render(shore_kind,here=6,sides=sides,mask=0)).getextrema()==(0,0))
check(len(set(shore_images))==24)
for sector in [0,1,2,3,4,5,6,8,9,10,11,12,13,14]:
    check(alpha(render(shore_kind,here=6,sides=(sector,)*4,mask=1)).getextrema()==(0,0))
# Rounded caps project past both ends. Unknown/blocked side rooms clip them.
for direction in range(4):
    sides=[6]*4;sides[direction]=7
    cap=render(shore_kind,here=6,sides=sides,mask=1<<direction,seed=3,wide=True,neighbours=255)
    cut=render(shore_kind,here=6,sides=sides,mask=1<<direction,seed=3,wide=True,neighbours=0)
    check(sum(alpha(cap).tobytes())>sum(alpha(cut).tobytes()))
    if direction<2:
        check(alpha(cap).crop((0,0,85,256)).getbbox() is not None)
        check(alpha(cap).crop((171,0,256,256)).getbbox() is not None)
    else:
        check(alpha(cap).crop((0,0,256,85)).getbbox() is not None)
        check(alpha(cap).crop((0,171,256,256)).getbbox() is not None)
    # A wall at a strip end prevents a cap bypassing that wall.
    wall=4 if direction<2 else 1
    blocked=render(shore_kind,here=6,sides=sides,mask=1<<direction,seed=3,
                   wide=True,neighbours=255,walls=wall)
    box=(171,0,256,256) if direction<2 else (0,171,256,256)
    check(alpha(blocked).crop(box).getbbox() is None)
# At an outward bend, the end from each adjoining water room reaches into
# the diagonal water room. Rounded cap shoulders cover the corner for every
# variant, including when the other room picked a different variant.
for direction in [1,3]:
    sides=[6]*4;sides[direction]=7
    for variant in range(24):
        cap=render(shore_kind,here=6,sides=sides,mask=1<<direction,seed=variant,
                   wide=True,neighbours=255)
        point=(173,89) if direction==1 else (89,173)
        check(cap.getpixel(point)[3]==255)

# No jagged procedural wave remains; clear water at the centre for all sides.
for direction in range(4):
    sides=[7]*4
    im=render(shore_kind,here=6,sides=sides,mask=1<<direction,neighbours=255)
    check(im.getpixel((128,128))[3]==0)

# Water undercoat always fills its room; patch shapes supply the visible variety.
check(alpha(render(9,here=6,mask=0)).getextrema() == (255,255))

# Road surfaces and undercoat share one opaque base instance, below land
# fringes; all directional masks still preserve a visible route centre.
for mask in range(1, 16):
    base = render(7, sides=(1,1,1,1), mask=mask)
    check(alpha(base).getextrema() == (255,255))
    # Full-room road art, without a substituted land material at corners.
    check(base.getpixel((128,128))[:3] == base.getpixel((0,0))[:3])

# Straight-run adjoining ends have identical alpha footprints for different
# seeds; road/trail joins share the same width from either side.
road = render(5,sides=(1,1,7,7),mask=12,seed=1)
next_road = render(5,sides=(1,1,7,7),mask=12,seed=65534)
check(alpha(road).crop((255,0,256,256)).tobytes() ==
      alpha(next_road).crop((0,0,1,256)).tobytes())
road_end = render(5,sides=(1,1,1,7),mask=12,seed=7)
trail_end = render(6,here=1,sides=(1,1,1,7),mask=12,seed=999)
check(alpha(road_end).crop((255,0,256,256)).tobytes() ==
      alpha(trail_end).crop((0,0,1,256)).tobytes())
check(road.tobytes() != next_road.tobytes())
check(render(5,seed=1).tobytes() == render(5,seed=1).tobytes())

# Sequential banks share end widths; no discontinuity where two water rooms
# touch the same field shoreline.
one = render(4,here=6,sides=(1,6,6,6),seed=3)
two = render(4,here=6,sides=(1,6,6,6),seed=333)
check(alpha(one).crop((255,0,256,256)).tobytes() ==
      alpha(two).crop((0,0,1,256)).tobytes())
summer = render(4,here=6,sides=(1,6,6,6),day=180)
winter = render(4,here=6,sides=(1,6,6,6),day=30)
check(sum(winter.getpixel((128,255))[:3]) > sum(summer.getpixel((128,255))[:3]) + 200)
check(render(5,day=-1).tobytes() == render(5,day=210).tobytes())

# Broad water patches cross only mapped water neighbours, including diagonals.
# The synthetic opaque material isolates the clipping from artwork alpha.
water_points = [(128,230),(128,25),(230,128),(25,128),
                (230,230),(25,230),(230,25),(25,25)]
for allowed in [0,255] + [1 << bit for bit in range(8)]:
    patch = render(11,here=0,sides=(0,0,0,0),mask=0,neighbours=allowed,size=3,wide=True)
    check(patch.getpixel((128,128))[3] == 255)
    for bit,point in enumerate(water_points):
        check((patch.getpixel(point)[3] == 255) == bool(allowed & (1 << bit)))
# Quiet base colour contains no periodically repeated texture pattern.
base = render(9,here=0,sides=(0,0,0,0))
check(base.getpixel((10,10)) == base.getpixel((200,200)))
check(base.tobytes() == render(9,here=0,sides=(0,0,0,0),seed=999).tobytes())

# Underground frames are 24 permanent variants, not seasonally replaced rock.
for family in [14,15,16]:
    for variant in range(24):
        fixed = render((8 if family == 16 else 2) | 4096,
                       here=family * 32 + variant,sides=(0,0,0,0),mask=0,day=0)
        for day in [90,180,270]:
            check(fixed.tobytes() == render((8 if family == 16 else 2) | 4096,
                  here=family * 32 + variant,sides=(0,0,0,0),mask=0,day=day).tobytes())
# Verify the ordinary modulation used for dark map rooms retains lighter tones.
for sector in [11,12]:
    lit = render(3 | 4096,here=sector).getpixel((128,128))
    dark = render(3 | 4096,here=sector,shade=.4).getpixel((128,128))
    check(all(abs(dark[i] - lit[i] * .4) <= 1 for i in range(3)))
    check(min(dark[:3]) > 40)

# Cave trails also stay fixed, using summer road/verge material, not snow.
for sector in [11,12]:
    fixed = render(6 | 4096,here=sector,day=0)
    check(fixed.tobytes() == render(6 | 4096,here=sector,day=270).tobytes())
    # Material at route centre equals the summer surface (same geometry).
    check(fixed.getpixel((128,128)) == render(6,here=sector,day=210).getpixel((128,128)))

# Ground reaches all four wall planes without an empty inset; overlap stops at walls.
# Kind 8 foreground deliberately remains uncut, because the CPU relocates it.
for kind in [2,3,7,9,11]:
    for direction in range(4):
        clipped = render(kind,here=0,sides=(0,0,0,0),mask=3,neighbours=255,walls=1 << direction)
        check(clipped.getpixel(points[direction])[3] > 0)
        check(clipped.getpixel((128,128))[3] > 240)
        if kind in [2,11]:
            outside = render(kind,here=0,sides=(0,0,0,0),mask=3,neighbours=255,
                             walls=1 << direction,size=3,wide=True)
            check(outside.getpixel(water_points[direction])[3] == 0)
for kind in [2,11]:
    for allowed in [0,255] + [1 << bit for bit in range(8)]:
        patch = render(kind,here=0,sides=(0,0,0,0),mask=0,neighbours=allowed,size=3,wide=True)
        for bit,point in enumerate([(128,210),(128,45),(210,128),(45,128), (210,210),(45,210),(210,45),(45,45)]):
            check((patch.getpixel(point)[3] > 0) == bool(allowed & (1 << bit)))
# Changing a sector behind a wall cannot recolour the road-side verge.
for direction in range(4):
    sides = [0]*4
    sides[direction] = 2
    road_wall = render(7,here=7,sides=sides,mask=3,walls=1 << direction)
    sides[direction] = 10
    check(road_wall.tobytes() == render(7,here=7,sides=sides,mask=3,
                                      walls=1 << direction).tobytes())
    check(road_wall.getpixel(points[direction])[3] == 255)
# Underwater uses its own fixed material family: no winter replacement.
underwater = render(3 | 4096,here=14,sides=(0,0,0,0),mask=0,day=0)
check(underwater.tobytes() == render(3 | 4096,here=14,sides=(0,0,0,0),mask=0,day=180).tobytes())
check(underwater.getpixel((128,128)) != render(3,here=6,sides=(0,0,0,0),mask=0).getpixel((128,128)))
for family,variant in [(25,0),(26,8)]:
    fixed = render((2 if family == 25 else 8) | 4096,here=family*32+variant,
                   sides=(0,0,0,0),mask=0,day=0)
    for day in [90,180,270]:
        check(fixed.tobytes() == render((2 if family == 25 else 8) | 4096,
              here=family*32+variant,sides=(0,0,0,0),mask=0,day=day).tobytes())
foreground = render(8,here=0,sides=(0,0,0,0),walls=15)
check(foreground.getpixel((128,128))[3] == 255)

# A room's explicit sky-shadow exclusion flag wins even for lit, untinted
# tree artwork; the same pass is used for solar and lunar projections.
check(alpha(render(0,here=0,sides=(0,0,0,0),shadow=True)).getbbox() is not None)
check(alpha(render(0 | 8192,here=0,sides=(0,0,0,0),shadow=True)).getbbox() is None)

# New upright silhouettes use the existing shadow pass, with shorter heights
# for brush/hills. Indoor exclusions still win for every new caster.
for height in [0,32768,65536]:
    caster = 8 | 16384 | height
    check(alpha(render(caster,here=0,sides=(0,0,0,0),shadow=True)).getbbox() is not None)
    check(alpha(render(caster | 8192,here=0,sides=(0,0,0,0),shadow=True)).getbbox() is None)
check(alpha(render(8,here=0,sides=(0,0,0,0),shadow=True)).getbbox() is None)
# Connection aprons cover the room edge but yield at the road's centre.
for direction in range(4):
    apron = render(5 | 32768,here=7,sides=(1,1,1,1),mask=1 << direction)
    check(apron.getpixel(points[direction])[3] >= 254)
    check(apron.getpixel((128,128))[3] == 0)
# Ground extending into a road gains an irregular narrow fringe, while its
# ownership mask is unchanged. A large synthetic patch isolates that mask.
plain = render(2,here=0,sides=(0,0,0,0),mask=0,neighbours=255,size=3,wide=True)
fringed = render(2 | (255 << 20),here=0,sides=(0,0,0,0),mask=0,
                 neighbours=255,size=3,wide=True)
check(alpha(fringed).getbbox() is not None)
check(sum(alpha(fringed).getdata()) < sum(alpha(plain).getdata()))
# The warning is a fixed room-owned image and cannot cast a sky shadow.
warning = 10 | 4096 | 8192
check(render(warning,here=28*32,sides=(0,0,0,0),mask=0,day=0).tobytes()
      == render(warning,here=28*32,sides=(0,0,0,0),mask=0,day=180).tobytes())
check(alpha(render(warning,here=28*32,sides=(0,0,0,0),mask=0,shadow=True)).getbbox() is None)

# Twilight grades actual material highlights; ink, indoor rooms, alpha and
# artificial illumination must remain unchanged. The default uniform is zero.
flags = 15 | 4096
raw = (19*32,0,0,flags)
neutral = render(flags,raw=raw,mask=0)
dark = render(flags,raw=raw,mask=0,shade=0.05)
GL.glUniform4f(GL.glGetUniformLocation(program,'uSkyTint'),0.70,0.55,0.35,0.80)
warm = render(flags,raw=raw,mask=0)
check(warm.tobytes() != neutral.tobytes())
check(alpha(warm).tobytes() == alpha(neutral).tobytes())
pixel = warm.getpixel((128,128)); original = neutral.getpixel((128,128))
check(pixel[0] > original[0] and pixel[2] < original[2])
check(dark.tobytes() == render(flags,raw=raw,mask=0,shade=0.05).tobytes())
inside = flags | 8192
check(render(inside,raw=(19*32,0,0,inside),mask=0).tobytes() == neutral.tobytes())
# A fully artificially lit outdoor room is also protected by the cached mask.
GL.glActiveTexture(GL.GL_TEXTURE1)
light_texture = GL.glGenTextures(1)
GL.glBindTexture(GL.GL_TEXTURE_2D,light_texture)
GL.glTexImage2D(GL.GL_TEXTURE_2D,0,GL.GL_RGBA8,1,1,0,GL.GL_RGBA,
                GL.GL_UNSIGNED_BYTE,bytes((255,180,0,255)))
GL.glTexParameteri(GL.GL_TEXTURE_2D,GL.GL_TEXTURE_MIN_FILTER,GL.GL_NEAREST)
GL.glTexParameteri(GL.GL_TEXTURE_2D,GL.GL_TEXTURE_MAG_FILTER,GL.GL_NEAREST)
GL.glActiveTexture(GL.GL_TEXTURE0)
GL.glUniform4i(GL.glGetUniformLocation(program,'uRoomLightGrid'),0,0,1,1)
GL.glUniform4f(GL.glGetUniformLocation(program,'uRoomLightParams'),1,0,0,0)
check(render(flags,raw=raw,mask=0).tobytes() == neutral.tobytes())
GL.glUniform4f(GL.glGetUniformLocation(program,'uRoomLightParams'),0,0,0,0)
GL.glUniform4f(GL.glGetUniformLocation(program,'uSkyTint'),0,0,0,0)
check(render(flags,raw=raw,mask=0).tobytes() == neutral.tobytes())

# The owning-room tint covers the complete sprite, including its overhang.
GL.glBindBuffer(GL.GL_UNIFORM_BUFFER,ubo)
dark_colour = (C.c_float*4)(.2,.25,.3,1.)
GL.glBufferSubData(GL.GL_UNIFORM_BUFFER,16,C.sizeof(dark_colour),dark_colour)
flags = 8192 # Tree, no sky shadows: isolate room tint from directional shading.
dark_tree = render(flags,raw=(0,1,255<<16,flags),size=1,mask=0,wide=True)
plain_tree = render(flags,raw=(0,0,255<<16,flags),size=1,mask=0,wide=True)
inside, outside = (128,154), (128,187)
check(dark_tree.getpixel(inside)[0] < plain_tree.getpixel(inside)[0]*.4)
check(dark_tree.getpixel(outside)[0] < plain_tree.getpixel(outside)[0]*.4)
check(alpha(dark_tree).tobytes() == alpha(plain_tree).tobytes())
# Panning and cached-light-grid changes must retain the full-sprite tint.
GL.glActiveTexture(GL.GL_TEXTURE1)
GL.glBindTexture(GL.GL_TEXTURE_2D,light_texture)
GL.glTexImage2D(GL.GL_TEXTURE_2D,0,GL.GL_RGBA8,16,16,0,GL.GL_RGBA,
                GL.GL_UNSIGNED_BYTE,bytes(16*16*4))
GL.glActiveTexture(GL.GL_TEXTURE0)
GL.glUniform4f(GL.glGetUniformLocation(program,'uRoomLightParams'),1,0,0,0)
panned = (C.c_float*16)(2/3,0,0,0, 0,2/3,0,0, 0,0,1,0, -.5,-1/6,0,1)
for origin in [(-1,-1,3,3),(0,0,2,2)]:
    GL.glUniform4i(GL.glGetUniformLocation(program,'uRoomLightGrid'),*origin)
    dark_pan = render(flags,raw=(0,1,255<<16,flags),size=1,mask=0,view_override=panned)
    plain_pan = render(flags,raw=(0,0,255<<16,flags),size=1,mask=0,view_override=panned)
    check(dark_pan.getpixel((107,175))[0] < plain_pan.getpixel((107,175))[0]*.4)
    check(dark_pan.getpixel((107,209))[0] < plain_pan.getpixel((107,209))[0]*.4)
GL.glUniform4f(GL.glGetUniformLocation(program,'uRoomLightParams'),0,0,0,0)

# Save a diagnostic contact sheet of actual shader output for visual inspection.
cases = [(5,7,(1,1,1,1),3),(5,7,(4,4,4,4),5),(6,1,(1,1,1,1),7),
         (4,6,(1,6,6,6),0),(4,6,(4,6,6,6),0),(4,6,(5,6,6,6),0)]
sheet = Image.new('RGBA',(768,512),(54,78,87,255))
for i,(kind,here,sides,mask) in enumerate(cases):
    sheet.alpha_composite(render(kind,here,sides,mask),((i%3)*256,(i//3)*256))
if len(sys.argv) > 2:
    sheet.transpose(Image.Transpose.FLIP_TOP_BOTTOM).save(sys.argv[2])
# A genuine inward strip, not a full-cell opaque material, tests NESW
# orientation and confirms that no sprite pixels leave the owning room.
strip = bytearray()
for page in range(2):
    for y in range(64):
        for x in range(64):
            strip.extend((165,145,120,255 if y % 16 >= 12 else 0))
GL.glTexImage3D(GL.GL_TEXTURE_2D_ARRAY,0,GL.GL_RGBA8,64,64,2,0,GL.GL_RGBA,GL.GL_UNSIGNED_BYTE,bytes(strip))
for direction,turn in enumerate([0,2,3,1]):
    wall = render(12 | 4096,here=0,sides=(0,0,0,0),mask=turn)
    check(wall.getpixel(points[direction])[3] == 255)
    check(wall.getpixel((128,128))[3] == 0)
    for other in range(4):
        if other != direction: check(wall.getpixel(points[other])[3] == 0)
    confined = render(12 | 4096,here=0,sides=(0,0,0,0),mask=turn,wide=True)
    bounds = alpha(confined).getbbox()
    check(bounds is not None and all(84 <= v <= 172 for v in bounds))
GL.glTexImage3D(GL.GL_TEXTURE_2D_ARRAY,0,GL.GL_RGBA8,64,64,2,0,GL.GL_RGBA,GL.GL_UNSIGNED_BYTE,bytes(64*64*2*4))
check(alpha(render(12 | 4096,here=0,sides=(0,0,0,0),mask=0)).getbbox() is None)
# Finishing pieces must not invent opaque posts when the optional art is blank.
for flags in [12|4096|65536|(18<<20)|(3<<18),12|4096|131072|(61<<20)|512]:
    check(alpha(render(flags,raw=(0,0,0,flags))).getbbox() is None)

# Optional real atlas verification: export with TestTerrainWalls OUTPUT_DIR,
# then pass OUTPUT_DIR as the third argument after the diagnostic PNG path.
if len(sys.argv) > 3:
    atlas_dir = Path(sys.argv[3])
    for level in range(8):
        pages = [Image.open(atlas_dir / f'wall-atlas-{page}-{level}.png').convert('RGBA') for page in range(4)]
        width,height = pages[0].size
        GL.glTexImage3D(GL.GL_TEXTURE_2D_ARRAY,level,GL.GL_RGBA8,width,height,4,0,
                        GL.GL_RGBA,GL.GL_UNSIGNED_BYTE,b''.join(page.tobytes() for page in pages))
    GL.glTexParameteri(GL.GL_TEXTURE_2D_ARRAY,GL.GL_TEXTURE_MAX_LEVEL,7)
    GL.glTexParameteri(GL.GL_TEXTURE_2D_ARRAY,GL.GL_TEXTURE_MIN_FILTER,GL.GL_LINEAR_MIPMAP_LINEAR)
    GL.glTexParameteri(GL.GL_TEXTURE_2D_ARRAY,GL.GL_TEXTURE_MAG_FILTER,GL.GL_LINEAR)
    examples = []
    for family in range(2):
        for variant in range(24):
            for direction,turn in enumerate([0,2,3,1]):
                wall = render(12 | 4096,here=family * 32 + variant,sides=(0,0,0,0),mask=turn,size=.33)
                point = [(128,169),(128,86),(169,128),(86,128)][direction]
                check(wall.getpixel(point)[3] >= 254)
                check(wall.getpixel((128,128))[3] == 0)
                if variant == 0: examples.append(wall)
    diagnostic = Image.new('RGBA',(1024,512),(105,101,91,255))
    for i,example in enumerate(examples):
        diagnostic.alpha_composite(example.transpose(Image.Transpose.FLIP_TOP_BOTTOM),((i%4)*256,(i//4)*256))
    if len(sys.argv) > 2: diagnostic.save(sys.argv[2])
# Outdoor receiver shadows union adjacent building segments, including
# duplicate sides of the same dividing wall, without darkening twice.
west_caster = 4 << 12  # east wall of offset (-1,0), grid cell 11.
wall = render(13, size=.5, raw=(0,0,west_caster,13 | 4096), shadow=True)
check(wall.getpixel((12,128))[3] > 60)
check(wall.getpixel((128,128))[3] == 0)
union = render(13, size=.5, raw=(0,0,west_caster | (8 << 16),13 | 4096), shadow=True)
check(union.tobytes() == wall.tobytes())
empty = render(13, size=.5, raw=(0,0,0,13 | 4096), shadow=True)
check(empty.getbbox() is None)
# The outer ring is needed at the capped low-sun slope; physical height
# changes its reach. Exercise both the first and last packed mask words.
low = render(13, size=.5, raw=(1 << 8,0,0,13 | 4096), shadow=True, sky=(0,3))
short = render(13, size=.25, raw=(1 << 8,0,0,13 | 4096), shadow=True, sky=(0,3))
check(low.getpixel((128,30))[3] > 60)
check(low.getpixel((128,200))[3] == 0)
check(short.getchannel('A').getbbox() is None)
last = render(13, size=.5, raw=(0,0,0,13 | 4096 | (2 << 4)), shadow=True, sky=(-3,-3))
check(last.getpixel((230,205))[3] > 60)
# Soft physical end caps cover a corner even when sunlight is exactly
# parallel to the wall (the old line projection skipped these segments).
parallel = render(13,size=.5,raw=(0,0,west_caster,13|4096),shadow=True,sky=(0,.8))
check(parallel.getpixel((1,1))[3] > 50)
check(wall.getpixel((1,1))[3] > 50)
check(sum(0 < wall.getpixel((x,128))[3] < 75 for x in range(100)) > 15)
# Grime is a translucent overlay: wall bands, stronger true corners, clean
# centres, and no band across an open edge.
one = render(14, raw=(0,0,1<<24,14 | 4096))
corner = render(14, raw=(0,0,9<<24,14 | 4096))
check(0 < one.getpixel((8,248))[3] < corner.getpixel((8,248))[3] < 150)
check(corner.getpixel((128,128))[3] == 0)
check(one.getpixel((128,8))[3] == 0)
# Grime reaches beyond the wider shingle face instead of hiding underneath.
eave_grime = render(14,raw=(61,0,1<<24,14|4096))
check(eave_grime.getpixel((128,180))[3] > 15)
check(eave_grime.getpixel((128,128))[3] == 0)
# A doorway end fades inside its room instead of exposing a hard cut at x=0.
tapered = render(14,raw=(61,0,(1<<24)|(1<<16),14|4096))
check(tapered.getpixel((0,230))[3] == 0)
check(0 < tapered.getpixel((12,230))[3] < tapered.getpixel((70,230))[3])
check(eave_grime.getpixel((0,230))[3] > 50) # continuous walls do not taper
if len(sys.argv) > 3:
    def upload_pages(indices):
        for level in range(8):
            pages = [Image.open(atlas_dir / f'wall-atlas-{page}-{level}.png').convert('RGBA') for page in indices]
            w,h = pages[0].size
            GL.glTexImage3D(GL.GL_TEXTURE_2D_ARRAY,level,GL.GL_RGBA8,w,h,len(pages),0,
                GL.GL_RGBA,GL.GL_UNSIGNED_BYTE,b''.join(p.tobytes() for p in pages))
    # A post finishes an actual wall end and a turned corner, but is absent
    # along a continuous run. It stays inside the owning room under rotation.
    upload_pages([4,5])
    post_style = 12|4096|65536|(18<<20)
    plain = render(post_style,raw=(0,0,0,post_style))
    post = render(post_style,raw=(0,0,0,post_style|(3<<18)))
    corner_post = render(post_style,raw=(0,0,9<<24,post_style))
    check(plain.getpixel((15,233))[3] == 0)
    check(post.getpixel((15,233))[3] > 200)
    check(corner_post.getpixel((15,233))[3] > 200)
    check(post.getpixel((128,128))[3] == 0)
    turned = render(post_style,raw=(0,0,0,post_style|(3<<18)|(1<<4)))
    from PIL import ImageChops
    difference = ImageChops.difference(post.transpose(Image.Transpose.ROTATE_270),turned)
    check(max(difference.getextrema()[i][1] for i in range(4)) <= 1)
    upload_pages([6,7,10,11])
    flags = 12|4096|131072|(61<<20)|1024
    north = render(flags,raw=(0,0,32,flags))
    # The cut wall edge must cover the retained MMapper wall beneath it,
    # while the outer shingle silhouette remains transparent and irregular.
    check(all(north.getpixel((x,255))[3] == 255 for x in range(256)))
    west = render(flags,raw=(0,0,32,flags|(1<<4)))
    from PIL import ImageChops
    check(max(ImageChops.difference(north.transpose(Image.Transpose.ROTATE_270),west).getextrema()[i][1] for i in range(4)) <= 1)
    # A convex corner return fills only its own small quadrant; the hip cap
    # fills the old diagonal mitre gap at concave corners as well.
    convex = render(flags,raw=(6,0,0,flags|512))
    check(convex.getpixel((20,235))[3] > 200)
    check(convex.getpixel((128,128))[3] == 0)
    hip = render(flags,raw=(0,0,(9<<24)|32,flags))
    check(hip.getpixel((35,217))[3] > 200)
    # The same corner is emitted by only one of its perpendicular strips.
    other = render(flags,raw=(0,0,(9<<24)|32,flags|(1<<4)))
    check(other.getpixel((35,217))[3] == 0)
    # Straight roofs and doorway ends are exact quarter turns, including
    # their source grain, courses and gradient selection.
    for ends in [0, 1, 2, 3]:
        style = flags | (ends<<18)
        original = render(style,raw=(0,0,32,style))
        for turns in [1,2,3]:
            rotated = render(style,raw=(0,0,32,style|(turns<<4)))
            expected = original.transpose([None,Image.Transpose.ROTATE_270,
                                           Image.Transpose.ROTATE_180,
                                           Image.Transpose.ROTATE_90][turns])
            check(max(ImageChops.difference(expected,rotated).getextrema()[i][1]
                      for i in range(4)) <= 1)
    # A decorative cap overlaps both adjoining roof strips in separate,
    # room-confined fragments. Its source frame stays continuous across them.
    upload_pages([10,11])
    cap_style = flags|2048|512
    centre = render(cap_style,raw=(0,0,0,cap_style))
    shoulder_n = render(cap_style,raw=(0,0,1<<16,cap_style))
    shoulder_w = render(cap_style,raw=(0,0,2<<16,cap_style))
    check(centre.getpixel((30,225))[3] > 128)
    check(centre.getpixel((128,128))[3] == 0)
    check(centre.getpixel((58,197))[3] == 0) # diagonal tip stays inside round footprint
    check(convex.getpixel((58,197))[3] == 0) # no old square underlay pokes through
    centre.save(atlas_dir.parent/'roof-centre.png')
    shoulder_n.save(atlas_dir.parent/'roof-shoulder-n.png')
    shoulder_w.save(atlas_dir.parent/'roof-shoulder-w.png')
    check(shoulder_n.getpixel((30,3))[3] > 64)
    check(shoulder_w.getpixel((253,225))[3] > 64)
    check(shoulder_n.getpixel((30,128))[3] == 0)
    check(shoulder_w.getpixel((128,225))[3] == 0)
    for fragment in [0,1,2]:
        original = render(cap_style,raw=(0,0,fragment<<16,cap_style))
        for turns in [1,2,3]:
            rotated = render(cap_style,raw=(0,0,fragment<<16,cap_style|(turns<<4)))
            expected = original.transpose([None,Image.Transpose.ROTATE_270,
                                           Image.Transpose.ROTATE_180,
                                           Image.Transpose.ROTATE_90][turns])
            check(max(ImageChops.difference(expected,rotated).getextrema()[i][1]
                      for i in range(4)) <= 1)
    # Variant four has another wall strip directly below it in the atlas.
    # At small scale coarse mips must still leave the room interior empty.
    for pages,depth in [([4,5],18),([8,9],28)]:
        upload_pages(pages)
        flags = 12|4096|(depth<<20)
        wall = render(flags,raw=(4,0,1<<24,flags),size=.33)
        check(wall.getpixel((128,128))[3] == 0)
        check(wall.getpixel((128,120))[3] == 0)
    # Exercise the actual warning artwork, not just a synthetic family colour.
    for level in range(8):
        pages=[Image.open(atlas_dir/f'warning-atlas-{page}-{level}.png').convert('RGBA') for page in range(2)]
        w,h=pages[0].size
        GL.glTexImage3D(GL.GL_TEXTURE_2D_ARRAY,level,GL.GL_RGBA8,w,h,2,0,
                        GL.GL_RGBA,GL.GL_UNSIGNED_BYTE,b''.join(p.tobytes() for p in pages))
    GL.glTexParameteri(GL.GL_TEXTURE_2D_ARRAY,GL.GL_TEXTURE_MAX_LEVEL,7)
    danger_flags=10|4096|8192
    danger=render(danger_flags,raw=(0,0,0,danger_flags),day=0)
    check(danger.getchannel('A').getextrema()[1] > 240)
    check(sum(a>200 for a in danger.getchannel('A').tobytes()) > 10000)
    check(danger.tobytes() == render(danger_flags,raw=(0,0,0,danger_flags),day=180).tobytes())
# An alpha silhouette with a sharp authored edge receives a feather in the
# existing tree shadow pass; nearby fully covered pixels retain their opacity.
half = bytearray()
for page in range(2):
    for y in range(64):
        for x in range(64): half.extend((170,140,100,255 if x % 16 >= 8 else 0))
GL.glTexImage3D(GL.GL_TEXTURE_2D_ARRAY,0,GL.GL_RGBA8,64,64,2,0,GL.GL_RGBA,GL.GL_UNSIGNED_BYTE,bytes(half))
GL.glTexParameteri(GL.GL_TEXTURE_2D_ARRAY,GL.GL_TEXTURE_MAX_LEVEL,0)
GL.glTexParameteri(GL.GL_TEXTURE_2D_ARRAY,GL.GL_TEXTURE_MIN_FILTER,GL.GL_NEAREST)
feather = render(4096,raw=(0,0,0,4096),shadow=True,sky=(0,1))
check(0 < feather.getpixel((127,180))[3] < 60)
check(feather.getpixel((150,180))[3] > 70)
# Directional face shading shares the sky vector with projected shadows.
# Backlighting darkens the front face without changing the alpha silhouette;
# indoor/no-sky props and non-casting detail keep their authored colours.
for kind in [0,8|16384,8|16384|32768,8|16384|65536]:
    flags=kind|4096
    neutral=render(flags,raw=(0,0,0,flags))
    front=render(flags,raw=(0,0,0,flags),sky=(0,1),light=.26)
    back=render(flags,raw=(0,0,0,flags),sky=(0,-1),light=.26)
    moon=render(flags,raw=(0,0,0,flags),sky=(0,-1),light=.05)
    point=(150,180)
    check(front.getpixel(point)[0] > back.getpixel(point)[0]+15)
    check(neutral.getpixel(point)[0] > moon.getpixel(point)[0] > back.getpixel(point)[0])
    check(alpha(front).tobytes() == alpha(back).tobytes() == alpha(neutral).tobytes())
    inside=flags|8192
    check(render(inside,raw=(0,0,0,inside),sky=(0,-1),light=.26).tobytes()
          == render(inside,raw=(0,0,0,inside)).tobytes())
for kind in [1,8,10]:
    flags=kind|4096
    check(render(flags,raw=(0,0,0,flags),sky=(0,-1),light=.26).tobytes()
          == render(flags,raw=(0,0,0,flags)).tobytes())

# A deliberately poisonous neighbour must not outline a minified roof.
# Explicit gradients alone cannot prevent bleed without a mip-sized guard.
poison = bytearray((240,255,0,255))*(512*512*2)
for y in range(128):
    for x in range(128):
        i=(y*512+x)*4
        poison[i:i+4]=bytes((20,50,90,255 if y>=97 else 0))
GL.glTexImage3D(GL.GL_TEXTURE_2D_ARRAY,0,GL.GL_RGBA8,512,512,2,0,GL.GL_RGBA,GL.GL_UNSIGNED_BYTE,bytes(poison))
GL.glTexParameteri(GL.GL_TEXTURE_2D_ARRAY,GL.GL_TEXTURE_MAX_LEVEL,7)
GL.glGenerateMipmap(GL.GL_TEXTURE_2D_ARRAY)
GL.glTexParameteri(GL.GL_TEXTURE_2D_ARRAY,GL.GL_TEXTURE_MIN_FILTER,GL.GL_LINEAR_MIPMAP_LINEAR)
roof = render(12|4096|131072|(61<<20),raw=(0,0,0,12|4096|131072|(61<<20)),size=.2)
check(roof.getpixel((128,153))[1] < 80)
check(roof.getpixel((128,153))[0] < 50)
check(roof.getpixel((128,154))[3] == 0)
# A whole two-room surface and the same surface split at a room edge must
# produce the same pixels: paving UVs cannot depend on individual quads.
def paving(split):
    count = 2 if split else 1
    instances = (Instance * count)()
    for i in range(count):
        instances[i].pos = (C.c_float*4)(.5+i if split else 1.,.5,1 if split else 2,0)
        instances[i].data = (C.c_int*4)(0,0,0,15|4096)
    GL.glBufferData(GL.GL_ARRAY_BUFFER,C.sizeof(instances),instances,GL.GL_STATIC_DRAW)
    view = (C.c_float*16)(1,0,0,0,0,2,0,0,0,0,1,0,-1,-1,0,1)
    GL.glUniformMatrix4fv(GL.glGetUniformLocation(program,'uMVP'),1,False,view)
    GL.glClear(GL.GL_COLOR_BUFFER_BIT)
    GL.glDrawArraysInstanced(GL.GL_TRIANGLE_STRIP,0,4,count)
    GL.glFinish()
    return bytes(GL.glReadPixels(0,0,256,256,GL.GL_RGBA,GL.GL_UNSIGNED_BYTE))
check(max(abs(a-b) for a,b in zip(paving(False),paving(True))) <= 1)
if len(sys.argv) > 3:
    atlas_dir = Path(sys.argv[3])
    GL.glTexParameteri(GL.GL_TEXTURE_2D_ARRAY,GL.GL_TEXTURE_MIN_FILTER,GL.GL_NEAREST)
    GL.glTexParameteri(GL.GL_TEXTURE_2D_ARRAY,GL.GL_TEXTURE_MAG_FILTER,GL.GL_NEAREST)
    door_pages = [Image.open(atlas_dir / f'door-atlas-{page}-0.png').convert('RGBA') for page in range(2)]
    GL.glTexImage3D(GL.GL_TEXTURE_2D_ARRAY,0,GL.GL_RGBA8,512,512,2,0,GL.GL_RGBA,
                    GL.GL_UNSIGNED_BYTE,b''.join(page.tobytes() for page in door_pages))
    review = Image.new('RGBA',(1536,320),(110,105,95,255))
    check(GL.glGetUniformLocation(program,'uTerrainDoors') == -1)
    for variant in range(6):
        flags = 12 | 4096 | 8192 | 256
        first = render(flags,raw=(variant,0,0,flags),mask=0)
        other_flags = flags | 512 | (2 << 4)
        second = render(other_flags,raw=(variant,0,0,other_flags),mask=0)
        check(alpha(first).getbbox() is not None)
        check(alpha(second).getbbox() is not None)
        # Both closed halves retain the exact canonical join and full artwork colour.
        for x in range(75,181,15):
            u = 0.25+0.5*(x+0.5)/256.0
            for image,y,v in [(first,255,0.5-0.25/256.0),(second,0,0.5+0.25/256.0)]:
                expected = door_pages[0].getpixel(((variant%4)*128+int(u*128),
                                                  (variant//4)*128+int(v*128)))
                actual = image.getpixel((x,y))
                check(max(abs(actual[c]-expected[c]) for c in range(4)) <= 1)
        pair = Image.new('RGBA',(256,512),(110,105,95,255))
        pair.alpha_composite(second.transpose(Image.Transpose.FLIP_TOP_BOTTOM),(0,0))
        pair.alpha_composite(first.transpose(Image.Transpose.FLIP_TOP_BOTTOM),(0,256))
        review.alpha_composite(pair.crop((0,128,256,448)),(variant*256,0))
        for side,turn in enumerate([0,2,3,1]):
            style=flags | (turn << 4)
            own=render(style,raw=(variant,0,0,style),mask=0)
            check(own.tobytes() == render(style,mask=0,reuse=True).tobytes())
        single=render(flags,raw=(variant,0,0,flags),mask=0)
        check(single.tobytes() == first.tobytes())
    review.save(atlas_dir.parent / ('doors-shader-review-' + api + '.png'))
    examples.append(first)
print('PASS', checks, 'render assertions')
EGL.eglMakeCurrent(display,EGL.EGL_NO_SURFACE,EGL.EGL_NO_SURFACE,EGL.EGL_NO_CONTEXT)
EGL.eglDestroyContext(display,context)
EGL.eglDestroySurface(display,surface)
EGL.eglTerminate(display)
