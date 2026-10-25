#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
# Copyright (C) 2026 The MMapper Authors
"""Exercise diffuse moonlight and moon-coloured specularity in production GL/GLES shaders."""
from pathlib import Path
import ctypes as C
import sys
import numpy as np
from PIL import Image

base = Path(__file__).with_name('reflections_render.py')
state = {'__file__': str(base), '__name__': '__main__'}
exec(compile(base.read_text().split('EGL = h["EGL"]')[0], str(base), 'exec'), state)
GL, render, upload = state['GL'], state['render'], state['upload']
uniform, difference = state['uniform'], state['difference']
checks = 0


def check(condition):
    global checks
    assert condition
    checks += 1


def moon(strength=1., direction=(.7, -.4, .59), mode=0):
    GL.glUniform4f(uniform('uMoonTint'), .62, .76, 1., strength)
    GL.glUniform4f(uniform('uReflectionLight'), *direction, .18 * strength)
    GL.glUniform4f(uniform('uReflectionColor'), .62, .76, 1., 1.5)
    GL.glUniform4i(uniform('uReflectionAtlas'), 0, 32, mode, 0)


def water(flags=0, **kwargs):
    return render(0, raw=(0, 0, 0, 11 | 4096 | flags), **kwargs)


colour = Image.new('RGBA', (512, 512), (100, 100, 100, 255))
normal = Image.new('RGBA', (512, 512), (65, 128, 239, 210))
upload([colour, colour, normal, normal])
GL.glUniform4f(uniform('uSkyTint'), 0., 0., 0., 0.)
GL.glUniform4f(uniform('uRoomLightParams'), 0., 0., 0., 0.)
moon(0.)
off = water()
moon()
on = water()
check(difference(off, on) > 2.)
check(np.array_equal(np.asarray(off)[:, :, 3], np.asarray(on)[:, :, 3]))
gain = np.asarray(on, dtype=float)[:, :, :3].mean((0, 1)) - np.asarray(off)[:, :, :3].mean((0, 1))
check(gain[2] > gain[1] > gain[0] > 0.)
moon(.25)
quarter_power = water()
check(0. < difference(off, quarter_power) < difference(off, on))
moon(0.)
check(water().tobytes() == off.tobytes())

# Composite the actual production night shader over the actual terrain draw.
fixture = state['h']
root = Path(__file__).resolve().parents[1]
night_program = GL.glCreateProgram()
for stage, kind in [('vert', GL.GL_VERTEX_SHADER), ('frag', GL.GL_FRAGMENT_SHADER)]:
    shader = GL.glCreateShader(kind)
    source = (root / f'src/resources/shaders/legacy/weather/timeofday/{stage}.glsl').read_text()
    if stage == 'frag':
        source = (root / 'src/resources/shaders/legacy/room-lighting.glsl').read_text() + '\n' + source
    GL.glShaderSource(shader, fixture['header'] + source)
    GL.glCompileShader(shader)
    check(GL.glGetShaderiv(shader, GL.GL_COMPILE_STATUS))
    GL.glAttachShader(night_program, shader)
GL.glLinkProgram(night_program)
check(GL.glGetProgramiv(night_program, GL.GL_LINK_STATUS))
GL.glUseProgram(night_program)
for name, binding, values in [('NamedColorsBlock', 0, [1.] * 1024),
                              ('TimeBlock', 1, [0.] * 4), ('WeatherBlock', 2, [0.] * 16)]:
    buffer = GL.glGenBuffers(1)
    GL.glBindBuffer(GL.GL_UNIFORM_BUFFER, buffer)
    data = (C.c_float * len(values))(*values)
    GL.glBufferData(GL.GL_UNIFORM_BUFFER, C.sizeof(data), data, GL.GL_STATIC_DRAW)
    index = GL.glGetUniformBlockIndex(night_program, name)
    GL.glUniformBlockBinding(night_program, index, binding)
    GL.glBindBufferBase(GL.GL_UNIFORM_BUFFER, binding, buffer)
GL.glUniform1i(GL.glGetUniformLocation(night_program, 'uRoomLights'), 1)
GL.glUniform4f(GL.glGetUniformLocation(night_program, 'uRoomLightParams'), 0., 0., 0., 0.)
GL.glUniform4f(GL.glGetUniformLocation(night_program, 'uSkyTint'), .10, .14, .22, .55)
terrain_vao = int(GL.glGetIntegerv(GL.GL_VERTEX_ARRAY_BINDING))
night_vao = GL.glGenVertexArrays(1)


def at_night(strength):
    GL.glUseProgram(fixture['program'])
    GL.glBindVertexArray(terrain_vao)
    moon(strength)
    water()
    GL.glUseProgram(night_program)
    GL.glBindVertexArray(night_vao)
    GL.glEnable(GL.GL_BLEND)
    GL.glBlendFunc(GL.GL_SRC_ALPHA, GL.GL_ONE_MINUS_SRC_ALPHA)
    GL.glDrawArrays(GL.GL_TRIANGLES, 0, 3)
    GL.glFinish()
    pixels = GL.glReadPixels(0, 0, 256, 256, GL.GL_RGBA, GL.GL_UNSIGNED_BYTE)
    GL.glDisable(GL.GL_BLEND)
    GL.glUseProgram(fixture['program'])
    GL.glBindVertexArray(terrain_vao)
    check(GL.glGetError() == GL.GL_NO_ERROR)
    return np.asarray(Image.frombytes('RGBA', (256, 256), bytes(pixels)), dtype=float)[:, :, :3]


night_off = at_night(0.)
night_on = at_night(1.)
check(np.abs(night_on - night_off).mean() > 1.)
check(night_on[:, :, 2].mean() - night_off[:, :, 2].mean()
      > night_on[:, :, 0].mean() - night_off[:, :, 0].mean())

# Indoor/permanent-shade flag and shadow passes keep their original output.
for flags in [8192]:
    moon(0.); dark = water(flags)
    moon(); check(water(flags).tobytes() == dark.tobytes())
moon(0.); shadow_off = water(shadow=True)
moon(); check(water(shadow=True).tobytes() == shadow_off.tobytes())

# Foreground relief responds to the moon's direction, even with specularity off.
def upright():
    return render(0, raw=(0, 0, 0, 8 | 4096 | 16384), pos=(.5, .06), light=0.)


moon(direction=(.95, 0., .31)); east = upright()
moon(direction=(-.95, 0., .31)); west = upright()
check(difference(east, west) > .3)
moon(direction=(0., .95, .31)); behind = upright()
moon(direction=(0., -.95, .31)); front = upright()
check(np.asarray(front)[:, :, :3].mean() > np.asarray(behind)[:, :, :3].mean())

# Reflection brightness/mode remain independent controls on top of the fill.
moon(); fill_only = water()
moon(mode=2); with_glints = water()
check(difference(fill_only, with_glints) > .15)
check(np.array_equal(np.asarray(fill_only)[:, :, 3], np.asarray(with_glints)[:, :, 3]))
moon(direction=(0., .8, .6), mode=2)
check(difference(with_glints, water()) > .1)

# An artificial-light pool suppresses the added cool fill, preserving warmth.
light_texture = GL.glGenTextures(1)
GL.glActiveTexture(GL.GL_TEXTURE1)
GL.glBindTexture(GL.GL_TEXTURE_2D, light_texture)
GL.glTexImage2D(GL.GL_TEXTURE_2D, 0, GL.GL_RGBA8, 1, 1, 0,
                GL.GL_RGBA, GL.GL_UNSIGNED_BYTE, bytes([0, 190, 0, 255]))
GL.glTexParameteri(GL.GL_TEXTURE_2D, GL.GL_TEXTURE_MIN_FILTER, GL.GL_NEAREST)
GL.glTexParameteri(GL.GL_TEXTURE_2D, GL.GL_TEXTURE_MAG_FILTER, GL.GL_NEAREST)
GL.glUniform4i(uniform('uRoomLightGrid'), 0, 0, 1, 1)
GL.glUniform4f(uniform('uRoomLightParams'), 1., 0., 0., 0.)
GL.glActiveTexture(GL.GL_TEXTURE0)
moon(0.); artificially_lit = water()
moon(); check(water().tobytes() == artificially_lit.tobytes())
print(f'PASS {checks} moonlight render assertions')

fixture = state['h']; EGL = fixture['EGL']
EGL.eglMakeCurrent(fixture['display'], EGL.EGL_NO_SURFACE, EGL.EGL_NO_SURFACE, EGL.EGL_NO_CONTEXT)
EGL.eglDestroyContext(fixture['display'], fixture['context'])
EGL.eglDestroySurface(fixture['display'], fixture['surface'])
EGL.eglTerminate(fixture['display'])
