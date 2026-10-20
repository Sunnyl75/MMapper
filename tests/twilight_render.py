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
surface=EGL.eglCreatePbufferSurface(display,configs[0],(EGL.EGLint*5)(EGL.EGL_WIDTH,16,EGL.EGL_HEIGHT,16,EGL.EGL_NONE))
context_attrs=(EGL.EGLint*3)(EGL.EGL_CONTEXT_CLIENT_VERSION,3,EGL.EGL_NONE) if es else (EGL.EGLint*7)(0x3098,3,0x30FB,3,0x30FD,1,EGL.EGL_NONE)
context=EGL.eglCreateContext(display,configs[0],EGL.EGL_NO_CONTEXT,context_attrs)
assert EGL.eglMakeCurrent(display,surface,surface,context)
print(api,GL.glGetString(GL.GL_VERSION).decode())
root=Path(__file__).resolve().parents[1]
header=('#version 300 es\nprecision highp float;\n' if es else '#version 330\n')+'#define MAX_NAMED_COLORS 256\n'
program=GL.glCreateProgram()
for name,kind in [('vert',GL.GL_VERTEX_SHADER),('frag',GL.GL_FRAGMENT_SHADER)]:
 shader=GL.glCreateShader(kind)
 GL.glShaderSource(shader,header+(root/f'src/resources/shaders/legacy/weather/timeofday/{name}.glsl').read_text())
 GL.glCompileShader(shader)
 assert GL.glGetShaderiv(shader,GL.GL_COMPILE_STATUS),GL.glGetShaderInfoLog(shader)
 GL.glAttachShader(program,shader)
GL.glLinkProgram(program)
assert GL.glGetProgramiv(program,GL.GL_LINK_STATUS),GL.glGetProgramInfoLog(program)
GL.glUseProgram(program);GL.glBindVertexArray(GL.glGenVertexArrays(1))
def block(name,binding,data):
 buffer=GL.glGenBuffers(1);GL.glBindBuffer(GL.GL_UNIFORM_BUFFER,buffer)
 values=(C.c_float*len(data))(*data)
 GL.glBufferData(GL.GL_UNIFORM_BUFFER,C.sizeof(values),values,GL.GL_STATIC_DRAW)
 index=GL.glGetUniformBlockIndex(program,name)
 assert index != GL.GL_INVALID_INDEX
 GL.glUniformBlockBinding(program,index,binding);GL.glBindBufferBase(GL.GL_UNIFORM_BUFFER,binding,buffer)
block('NamedColorsBlock',0,[.1,.2,.4,.35,.4,.2,.1,.15]+[0.]*(1024-8))
block('TimeBlock',1,[1.,0.,0.,0.])
block('WeatherBlock',2,[0.]*8+[0.,1.,.4,.8]+[0.,0.,2.,0.])
GL.glViewport(0,0,16,16)
checks=0
def check(test):
 global checks
 assert test
 checks+=1

def render(tint):
 GL.glUniform4f(GL.glGetUniformLocation(program,'uSkyTint'),*tint)
 GL.glClearColor(0.,0.,0.,0.);GL.glClear(GL.GL_COLOR_BUFFER_BIT)
 GL.glDrawArrays(GL.GL_TRIANGLES,0,3);GL.glFinish()
 assert GL.glGetError()==GL.GL_NO_ERROR
 return bytes(GL.glReadPixels(8,8,1,1,GL.GL_RGBA,GL.GL_UNSIGNED_BYTE))
# Uniform-only changes replace the old overlay, rather than layering twice.
for tint in [(0.,0.,0.,0.),(.05,.05,.2,.35),(1.,.75,.34,.035),
             (.76,.34,.49,.13),(.32,.25,.47,.245),(1.,.83,.52,.01)]:
 pixel=render(tint)
 check(max(abs(pixel[i]-round(tint[i]*255)) for i in range(4)) <= 1)
previous=render((.05,.05,.2,.35))
check(previous == render((.05,.05,.2,.35)))
# Unknown-clock mode retains the prior named-colour/intensity transition.
fallback=render((0.,0.,0.,-1.))
check(abs(fallback[3]-round(.25*.6*255)) <= 1)
check(fallback[0] > 0 and fallback[1] > 0 and fallback[2] > 0)
check(fallback != render((0.,0.,0.,0.)))
print('PASS',checks,'time-of-day shader assertions')
EGL.eglMakeCurrent(display,EGL.EGL_NO_SURFACE,EGL.EGL_NO_SURFACE,EGL.EGL_NO_CONTEXT)
EGL.eglDestroyContext(display,context);EGL.eglDestroySurface(display,surface);EGL.eglTerminate(display)
