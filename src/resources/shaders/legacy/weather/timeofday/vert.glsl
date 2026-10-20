// SPDX-License-Identifier: GPL-2.0-or-later
uniform mat4 uRoomLightInverse;
uniform vec4 uRoomLightParams;
out vec3 vRoomLightPlane;
void main()
{
    vec2 ndc = vec2((gl_VertexID << 1) & 2, gl_VertexID & 2) * 2.0 - 1.0;
    gl_Position = vec4(ndc,0.0,1.0);
    // Interpolate a homogeneous ray/plane intersection. Two matrix multiplies
    // per vertex, rather than per framebuffer pixel; also handles tilted maps.
    vec4 nearH = uRoomLightInverse*vec4(ndc,-1.0,1.0);
    vec4 farH = uRoomLightInverse*vec4(ndc,1.0,1.0);
    float layer = uRoomLightParams.w;
    vec4 planeH = nearH*(farH.z-layer*farH.w)-farH*(nearH.z-layer*nearH.w);
    vRoomLightPlane = planeH.xyw;
}
