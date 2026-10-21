#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
# Copyright (C) 2026 The MMapper Authors
"""Exercise the production time-of-day shader on desktop GL or GLES."""
import os
os.environ['PYOPENGL_PLATFORM'] = 'egl'
import ctypes as C
from pathlib import Path
import sys
from OpenGL import EGL, GL
from OpenGL.EGL.EXT.platform_base import eglGetPlatformDisplayEXT

api = sys.argv[1] if len(sys.argv)>1 else 'gl'
es = api == 'es'
display = eglGetPlatformDisplayEXT(0x31DD,EGL.EGL_DEFAULT_DISPLAY,None)
major,minor=EGL.EGLint(),EGL.EGLint()
assert EGL.eglInitialize(display,major,minor)
assert EGL.eglBindAPI(EGL.EGL_OPENGL_ES_API if es else EGL.EGL_OPENGL_API)
attrs=(EGL.EGLint*13)(EGL.EGL_SURFACE_TYPE,EGL.EGL_PBUFFER_BIT,EGL.EGL_RENDERABLE_TYPE,
  0x0040 if es else EGL.EGL_OPENGL_BIT,EGL.EGL_RED_SIZE,8,EGL.EGL_GREEN_SIZE,8,
  EGL.EGL_BLUE_SIZE,8,EGL.EGL_ALPHA_SIZE,8,EGL.EGL_NONE)
configs=(EGL.EGLConfig*1)();count=EGL.EGLint()
assert EGL.eglChooseConfig(display,attrs,configs,1,count) and count.value
surface=EGL.eglCreatePbufferSurface(display,configs[0],(EGL.EGLint*5)(EGL.EGL_WIDTH,128,EGL.EGL_HEIGHT,64,EGL.EGL_NONE))
context_attrs=(EGL.EGLint*3)(EGL.EGL_CONTEXT_CLIENT_VERSION,3,EGL.EGL_NONE) if es else (EGL.EGLint*7)(0x3098,3,0x30FB,3,0x30FD,1,EGL.EGL_NONE)
context=EGL.eglCreateContext(display,configs[0],EGL.EGL_NO_CONTEXT,context_attrs)
assert EGL.eglMakeCurrent(display,surface,surface,context)
print(api,GL.glGetString(GL.GL_VERSION).decode())
root=Path(__file__).resolve().parents[1]
header=('#version 300 es\nprecision highp float;\nprecision mediump sampler2DArray;\n' if es else '#version 330\n')+'#define MAX_NAMED_COLORS 256\n'

shared=(root/'src/resources/shaders/legacy/room-lighting.glsl').read_text()
checks=0
def check(value):
 global checks
 assert value
 checks+=1

def compile_program(directory):
 program=GL.glCreateProgram()
 for stage,kind in [('vert',GL.GL_VERTEX_SHADER),('frag',GL.GL_FRAGMENT_SHADER)]:
  shader=GL.glCreateShader(kind)
  source=(root/f'src/resources/shaders/legacy/{directory}/{stage}.glsl').read_text()
  if stage=='frag': source=shared+'\n'+source
  GL.glShaderSource(shader,header+source);GL.glCompileShader(shader)
  assert GL.glGetShaderiv(shader,GL.GL_COMPILE_STATUS),GL.glGetShaderInfoLog(shader)
  GL.glAttachShader(program,shader)
 GL.glLinkProgram(program)
 assert GL.glGetProgramiv(program,GL.GL_LINK_STATUS),GL.glGetProgramInfoLog(program)
 return program
# All three actual consumers must compile and link with the same helper.
programs={name:compile_program(name) for name in ['weather/timeofday','room/tex/acolor','terrain']}
program=programs['weather/timeofday'];GL.glUseProgram(program)
GL.glBindVertexArray(GL.glGenVertexArrays(1));GL.glViewport(0,0,128,64)
GL.glUniform1i(GL.glGetUniformLocation(program,'uRoomLights'),1)
tex=GL.glGenTextures(1);GL.glActiveTexture(GL.GL_TEXTURE1);GL.glBindTexture(GL.GL_TEXTURE_2D,tex)
def grid(a,b,shadow=0):
 data=bytearray()
 for y in range(16):
  for x in range(32):
   room=a if x<16 else b
   edge=min(x%16,y,15-x%16,15-y)
   alpha=room[0] if edge>0 else 0
   data.extend((255,190,shadow if alpha else 0,alpha))
 GL.glActiveTexture(GL.GL_TEXTURE1);GL.glBindTexture(GL.GL_TEXTURE_2D,tex)
 GL.glTexImage2D(GL.GL_TEXTURE_2D,0,GL.GL_RGBA8,32,16,0,GL.GL_RGBA,GL.GL_UNSIGNED_BYTE,bytes(data))
 GL.glTexParameteri(GL.GL_TEXTURE_2D,GL.GL_TEXTURE_MIN_FILTER,GL.GL_LINEAR)
 GL.glTexParameteri(GL.GL_TEXTURE_2D,GL.GL_TEXTURE_MAG_FILTER,GL.GL_LINEAR)
 GL.glTexParameteri(GL.GL_TEXTURE_2D,GL.GL_TEXTURE_WRAP_S,GL.GL_CLAMP_TO_EDGE)
 GL.glTexParameteri(GL.GL_TEXTURE_2D,GL.GL_TEXTURE_WRAP_T,GL.GL_CLAMP_TO_EDGE)
grid([230,100,20,110],[0,0,0,0])
GL.glUniform4i(GL.glGetUniformLocation(program,'uRoomLightGrid'),0,0,2,1)
GL.glUniform4f(GL.glGetUniformLocation(program,'uRoomLightViewport'),0,0,128,64)
# Column-major inverse projection: NDC covers world x=0..2, y=0..1.
inverse=(C.c_float*16)(1,0,0,0, 0,.5,0,0, 0,0,1,0, 1,.5,0,1)
GL.glUniformMatrix4fv(GL.glGetUniformLocation(program,'uRoomLightInverse'),1,False,inverse)

def render(strength=1.,flicker=.35,time=0.,live=False,tint=(.03,.04,.12,.6)):
 GL.glUniform4f(GL.glGetUniformLocation(program,'uRoomLightParams'),strength,flicker,time,0.)
 GL.glUniform4f(GL.glGetUniformLocation(program,'uRoomLiveLight'),1.,0.,float(live),0.)
 GL.glUniform4f(GL.glGetUniformLocation(program,'uSkyTint'),*tint)
 GL.glClearColor(0,0,0,0);GL.glClear(GL.GL_COLOR_BUFFER_BIT)
 GL.glDrawArrays(GL.GL_TRIANGLES,0,3);GL.glFinish()
 assert GL.glGetError()==GL.GL_NO_ERROR
 pixels=bytes(GL.glReadPixels(0,0,128,64,GL.GL_RGBA,GL.GL_UNSIGNED_BYTE))
 return lambda x,y: pixels[(y*128+x)*4:(y*128+x)*4+4]

unlit=render(strength=0)
check(unlit(32,32)==unlit(96,32))
check(abs(unlit(32,32)[3]-153)<=1)
night=render()
check(night(32,32)[3]<unlit(32,32)[3]//2)
check(night(96,32)==unlit(96,32))
check(night(32,32)[0]>night(96,32)[0])
# Room boundaries retain the overlay; no light leaks across a wall.
check(night(64,32)==unlit(64,32))
check(night(63,32)[3]>night(32,32)[3])
# Cached pixels are unchanged when a renderer advances its clock. Animation
# changes the texture on the CPU; the shader itself has no time dependence.
check(render(flicker=0,time=0)(32,32)==render(flicker=1,time=125)(32,32))
# Lit rooms have a restrained warm tint in daylight; disabled is exactly transparent.
day=render(tint=(0,0,0,0),flicker=0)
check(0<day(32,32)[3]<=62)
check(day(32,32)[0]>day(32,32)[1]>day(32,32)[2])
check(render(strength=0,tint=(0,0,0,0))(32,32)==bytes([0,0,0,0]))
# Corner-shadow data must visibly darken the light tint even in daylight;
# it is independent of the night overlay and cannot affect an unlit neighbor.
grid([230,0,0,0],[0,0,0,0],shadow=80)
shade=render(tint=(0,0,0,0))
check(shade(32,32)[0]<day(32,32)[0])
check(shade(32,32)[3]>day(32,32)[3])
check(shade(96,32)==bytes([0,0,0,0]))
# The same mask must reduce twilight, not replace its colour outside lit rooms.
grid([230,100,20,110],[0,0,0,0])
twilight=render(tint=(.76,.34,.49,.25));plain=render(strength=0,tint=(.76,.34,.49,.25))
check(twilight(96,32)==plain(96,32))
check(twilight(32,32)[3]<plain(32,32)[3])
# Render the actual multiplicative room-tint and organic-terrain programs too.
# This verifies relief of room darkness, rather than only the later sky overlay.
import struct
colors=[1.,1.,1.,1., .2,.2,.25,1.]+[0.]*(1024-8)
ubo=GL.glGenBuffers(1);GL.glBindBuffer(GL.GL_UNIFORM_BUFFER,ubo)
values=(C.c_float*len(colors))(*colors)
GL.glBufferData(GL.GL_UNIFORM_BUFFER,C.sizeof(values),values,GL.GL_STATIC_DRAW)
GL.glBindBufferBase(GL.GL_UNIFORM_BUFFER,0,ubo)
white=GL.glGenTextures(1);GL.glActiveTexture(GL.GL_TEXTURE0)
GL.glBindTexture(GL.GL_TEXTURE_2D_ARRAY,white)
GL.glTexImage3D(GL.GL_TEXTURE_2D_ARRAY,0,GL.GL_RGBA8,1,1,1,0,GL.GL_RGBA,GL.GL_UNSIGNED_BYTE,bytes([255]*4))
GL.glTexParameteri(GL.GL_TEXTURE_2D_ARRAY,GL.GL_TEXTURE_MIN_FILTER,GL.GL_NEAREST)
GL.glTexParameteri(GL.GL_TEXTURE_2D_ARRAY,GL.GL_TEXTURE_MAG_FILTER,GL.GL_NEAREST)
mvp=(C.c_float*16)(2,0,0,0, 0,2,0,0, 0,0,1,0, -1,-1,0,1)
surface_vaos={}
for name in ['room/tex/acolor','terrain']:
 current=programs[name];GL.glUseProgram(current)
 block_index=GL.glGetUniformBlockIndex(current,'NamedColorsBlock')
 GL.glUniformBlockBinding(current,block_index,0)
 GL.glUniform1i(GL.glGetUniformLocation(current,'uTexture'),0)
 GL.glUniform1i(GL.glGetUniformLocation(current,'uRoomLights'),1)
 GL.glUniform4i(GL.glGetUniformLocation(current,'uRoomLightGrid'),0,0,2,1)
 GL.glUniform4f(GL.glGetUniformLocation(current,'uRoomLiveLight'),0,0,0,0)
 GL.glUniform4f(GL.glGetUniformLocation(current,'uColor'),1,1,1,1)
 GL.glUniformMatrix4fv(GL.glGetUniformLocation(current,'uMVP'),1,False,mvp)
 vao=GL.glGenVertexArrays(1);GL.glBindVertexArray(vao)
 surface_vaos[name]=vao
 vbo=GL.glGenBuffers(1);GL.glBindBuffer(GL.GL_ARRAY_BUFFER,vbo)
 if name=='room/tex/acolor':
  payload=struct.pack('<4i',0,0,0,256)
  GL.glBufferData(GL.GL_ARRAY_BUFFER,len(payload),payload,GL.GL_STATIC_DRAW)
  GL.glVertexAttribIPointer(0,4,GL.GL_INT,16,C.c_void_p(0));GL.glEnableVertexAttribArray(0)
  GL.glVertexAttribDivisor(0,1)
  GL.glUniform1i(GL.glGetUniformLocation(current,'uRoomLightTintPass'),1)
 else:
  payload=struct.pack('<4f4i',.5,.5,1.,0.,0,1,0,15|4096)
  GL.glBufferData(GL.GL_ARRAY_BUFFER,len(payload),payload,GL.GL_STATIC_DRAW)
  GL.glVertexAttribPointer(0,4,GL.GL_FLOAT,False,32,C.c_void_p(0))
  GL.glVertexAttribIPointer(1,4,GL.GL_INT,32,C.c_void_p(16))
  for index in [0,1]:
   GL.glEnableVertexAttribArray(index);GL.glVertexAttribDivisor(index,1)
  GL.glUniform1f(GL.glGetUniformLocation(current,'uTerrainDay'),-1.)
  GL.glUniform4f(GL.glGetUniformLocation(current,'uTerrainShadow'),0,0,0,0)
 def surface_pixel(strength,layer=0.):
  GL.glUniform4f(GL.glGetUniformLocation(current,'uRoomLightParams'),strength,0,0,layer)
  GL.glClearColor(0,0,0,0);GL.glClear(GL.GL_COLOR_BUFFER_BIT)
  GL.glDrawArraysInstanced(GL.GL_TRIANGLE_STRIP,0,4,1);GL.glFinish()
  assert GL.glGetError()==GL.GL_NO_ERROR
  return bytes(GL.glReadPixels(64,32,1,1,GL.GL_RGBA,GL.GL_UNSIGNED_BYTE))
 dark=surface_pixel(0);bright=surface_pixel(1)
 check(abs(dark[0]-51)<=1)
 check(bright[0]>dark[0]+120)
 check(surface_pixel(1,layer=1.)==dark) # Other elevations keep their own tint.
 if name=='room/tex/acolor':
  GL.glUniform1i(GL.glGetUniformLocation(current,'uRoomLightTintPass'),0)
  check(surface_pixel(1)==dark) # Ordinary coloured room passes are unchanged.
# Exercise the real organic-ground/native-tint EQUAL depth combination while
# panning and tilting. A non-depth pbuffer would miss this integration entirely.
if es:
 clear_depth=C.CFUNCTYPE(None,C.c_float)(EGL.eglGetProcAddress(b"glClearDepthf"))
else:
 clear_depth=GL.glClearDepth
fbo=GL.glGenFramebuffers(1);GL.glBindFramebuffer(GL.GL_FRAMEBUFFER,fbo)
colour_attachment=GL.glGenTextures(1);GL.glActiveTexture(GL.GL_TEXTURE2)
GL.glBindTexture(GL.GL_TEXTURE_2D,colour_attachment)
GL.glTexImage2D(GL.GL_TEXTURE_2D,0,GL.GL_RGBA8,128,64,0,GL.GL_RGBA,GL.GL_UNSIGNED_BYTE,None)
GL.glFramebufferTexture2D(GL.GL_FRAMEBUFFER,GL.GL_COLOR_ATTACHMENT0,GL.GL_TEXTURE_2D,colour_attachment,0)
depth_attachment=GL.glGenRenderbuffers(1);GL.glBindRenderbuffer(GL.GL_RENDERBUFFER,depth_attachment)
GL.glRenderbufferStorage(GL.GL_RENDERBUFFER,GL.GL_DEPTH_COMPONENT24,128,64)
GL.glFramebufferRenderbuffer(GL.GL_FRAMEBUFFER,GL.GL_DEPTH_ATTACHMENT,GL.GL_RENDERBUFFER,depth_attachment)
check(GL.glCheckFramebufferStatus(GL.GL_FRAMEBUFFER)==GL.GL_FRAMEBUFFER_COMPLETE)
# Only the room-tint pass is dark: an opaque white organic base must receive it.
GL.glBindVertexArray(surface_vaos['terrain'])
GL.glBindBuffer(GL.GL_ARRAY_BUFFER,vbo)
base_payload=struct.pack('<4f4i',.5,.5,1.,0.,0,0,0,15|4096)
GL.glBufferData(GL.GL_ARRAY_BUFFER,len(base_payload),base_payload,GL.GL_STATIC_DRAW)
# A zero illumination mask must preserve darkness both inside and outside its grid.
GL.glActiveTexture(GL.GL_TEXTURE1);GL.glBindTexture(GL.GL_TEXTURE_2D,tex)
GL.glTexImage2D(GL.GL_TEXTURE_2D,0,GL.GL_RGBA8,16,16,0,GL.GL_RGBA,GL.GL_UNSIGNED_BYTE,bytes(16*16*4))
GL.glActiveTexture(GL.GL_TEXTURE0);GL.glBindTexture(GL.GL_TEXTURE_2D_ARRAY,white)
for light_enabled in [0,1]:
 for grid_origin in [(-3,-3,8,8),(10,10,2,2)]:
  for pan_x,pan_y in [(-.7,-.4),(0.,0.),(.7,.4)]:
   view=(C.c_float*16)(.7,.2,.11,0, -.15,1,.17,0, 0,0,1,0, -.275+pan_x,-.6+pan_y,-.14,1)
   GL.glDisable(GL.GL_BLEND);GL.glEnable(GL.GL_DEPTH_TEST)
   GL.glDepthMask(True);GL.glDepthFunc(GL.GL_LESS)
   GL.glClearColor(0,0,0,0);clear_depth(1.)
   GL.glClear(GL.GL_COLOR_BUFFER_BIT|GL.GL_DEPTH_BUFFER_BIT)
   for name in ['terrain','room/tex/acolor']:
    current=programs[name];GL.glUseProgram(current);GL.glBindVertexArray(surface_vaos[name])
    GL.glUniformMatrix4fv(GL.glGetUniformLocation(current,'uMVP'),1,False,view)
    GL.glUniform4i(GL.glGetUniformLocation(current,'uRoomLightGrid'),*grid_origin)
    GL.glUniform4f(GL.glGetUniformLocation(current,'uRoomLightParams'),light_enabled,0,0,0)
    if name=='room/tex/acolor':
     GL.glUniform1i(GL.glGetUniformLocation(current,'uRoomLightTintPass'),1)
     GL.glDepthFunc(GL.GL_EQUAL);GL.glEnable(GL.GL_BLEND)
     GL.glBlendFunc(GL.GL_DST_COLOR,GL.GL_ZERO)
    GL.glDrawArraysInstanced(GL.GL_TRIANGLE_STRIP,0,4,1)
   GL.glFinish();check(GL.glGetError()==GL.GL_NO_ERROR)
   pixel=bytes(GL.glReadPixels(int((pan_x+1)*64),int((pan_y+1)*32),1,1,GL.GL_RGBA,GL.GL_UNSIGNED_BYTE))
   check(abs(pixel[0]-51)<=1 and abs(pixel[1]-51)<=1 and abs(pixel[2]-64)<=1)
GL.glDisable(GL.GL_DEPTH_TEST);GL.glDisable(GL.GL_BLEND)
GL.glBindFramebuffer(GL.GL_FRAMEBUFFER,0)
GL.glDeleteRenderbuffers(1,[depth_attachment]);GL.glDeleteFramebuffers(1,[fbo])
print('PASS',checks,'room lighting render assertions')
EGL.eglMakeCurrent(display,EGL.EGL_NO_SURFACE,EGL.EGL_NO_SURFACE,EGL.EGL_NO_CONTEXT)
EGL.eglDestroyContext(display,context);EGL.eglDestroySurface(display,surface);EGL.eglTerminate(display)
