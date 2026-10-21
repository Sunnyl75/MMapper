// SPDX-License-Identifier: GPL-2.0-or-later
precision highp float;
precision highp int;
uniform sampler2D uRoomLights;
uniform ivec4 uRoomLightGrid; // origin and extent in rooms
uniform vec4 uRoomLightParams; // enabled, reserved, reserved, focused layer
uniform int uRoomLightTintPass;

vec4 roomIllumination(vec3 world)
{
    if (uRoomLightParams.x <= 0.0 || abs(world.z-uRoomLightParams.w) > 0.01)
        return vec4(0.0);
    vec2 uv = (world.xy-vec2(uRoomLightGrid.xy))/vec2(uRoomLightGrid.zw);
    if (any(lessThan(uv,vec2(0.0))) || any(greaterThanEqual(uv,vec2(1.0))))
        return vec4(0.0);
    return texture(uRoomLights,uv);
}
