// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 The MMapper Authors
precision highp int;
uniform mat4 uMVP;
uniform vec4 uTerrainShadow;
uniform ivec4 uTerrainDoors;
uniform float uTerrainDay;
layout(std140) uniform NamedColorsBlock
{
    vec4 uNamedColors[MAX_NAMED_COLORS];
};
layout(location = 0) in vec4 aAnchorSizeLayer;
layout(location = 1) in ivec4 aTextureColor;
out vec2 vTreeUV;
flat out ivec2 vTiles;
flat out float vSeasonMix;
out vec4 vColor;
flat out int vKind;
flat out vec3 vBaseColor;
flat out ivec3 vSurface;
flat out float vWinter;
out vec2 vWorldUV;
flat out ivec2 vGroundSeasons;
flat out float vGroundMix;
flat out int vSharedBase;
flat out ivec3 vWaterClip;
flat out int vWallMask;
flat out uvec4 vShadowWalls;
flat out int vWallStyle;
flat out float vWallHeight;
flat out int vGroundRoads;
void main()
{
    const vec2 corners[4] = vec2[4](vec2(0, 0), vec2(1, 0), vec2(0, 1), vec2(1, 1));
    vec2 uv = corners[gl_VertexID];
    int kind = aTextureColor.w & 15;
    vKind = kind;
    bool castsShadow = kind == 0 || (kind == 8 && (aTextureColor.w & 16384) != 0);
    vGroundRoads = (aTextureColor.w >> 20) & 255;
    vWallStyle = aTextureColor.w;
    vWallHeight = aAnchorSizeLayer.z;
    vShadowWalls = uvec4(uint(aTextureColor.x), uint(aTextureColor.z),
                        uint(aTextureColor.y), uint(aTextureColor.w >> 4) & 15u);
    // All six images share the trunk anchor at (0.5, 0.06) from bottom-left.
    vec2 p = aAnchorSizeLayer.xy + (uv - vec2(0.5, 0.06)) * aAnchorSizeLayer.z;
    if (kind >= 2 && kind != 8) {
        vec2 offset = uv - vec2(0.5);
        int rotation = (kind == 2 || kind == 12) ? (aTextureColor.w >> 4) & 3 : 0;
        for (int i = 0; i < rotation; ++i) offset = vec2(-offset.y, offset.x);
        p = aAnchorSizeLayer.xy + offset * (kind == 13 ? 1.0 : aAnchorSizeLayer.z);
    }
    if (uTerrainShadow.w > 0.5 && castsShadow) {
        vec2 direction = uTerrainShadow.xy;
        if (kind == 8) {
            int height = (aTextureColor.w >> 15) & 3;
            direction *= height == 1 ? 0.35 : height == 2 ? 0.55 : 1.0;
        }
        vec2 across = normalize(vec2(direction.y, -direction.x));
        p = aAnchorSizeLayer.xy + aAnchorSizeLayer.z
            * (across * (uv.x - 0.5) * 0.75 + direction * (uv.y - 0.06));
    }
    gl_Position = uMVP * vec4(p, aAnchorSizeLayer.w, 1.0);
    // Day zero starts winter; successive seasons start on days 90, 180, 270.
    // Stagger tree centres over 24 days, with a four-day fade per tree.
    int previous = 2;
    int following = 2;
    float fade = 0.0;
    float baseFade = 0.0;
    if (uTerrainDay >= 0.0) {
        float boundary = floor((uTerrainDay + 14.0) / 90.0);
        float progress = (uTerrainDay - boundary * 90.0 + 14.0) / 28.0;
        baseFade = clamp(progress, 0.0, 1.0);
        following = int(mod(boundary, 4.0));
        previous = (following + 3) % 4;
        float start = float(aTextureColor.z & 65535) / 65535.0 * 24.0;
        fade = clamp((progress * 28.0 - start) / 4.0, 0.0, 1.0);
    }
    bool allSeason = (aTextureColor.w & 4096) != 0;
    if (allSeason) {
        previous = 0;
        following = 0;
        fade = 0.0;
        baseFade = 0.0;
    }
    const vec3 groundColors[4] = vec3[4](vec3(0.89, 0.91, 0.93),
        vec3(0.35, 0.42, 0.18), vec3(0.36, 0.40, 0.18), vec3(0.50, 0.43, 0.24));
    vBaseColor = mix(groundColors[previous], groundColors[following], baseFade);
    vSurface = ivec3(aTextureColor.x, aTextureColor.z, (aTextureColor.w >> 4) & 271); // Route mask plus all-season bit.
    vSharedBase = (aTextureColor.x >> 20) & 2047;
    vGroundSeasons = ivec2(previous, following);
    vGroundMix = baseFade;
    vWorldUV = p;
    vWallMask = (aTextureColor.z >> 24) & 15;
    vWaterClip = ivec3(ivec2(floor(aAnchorSizeLayer.xy)), (aTextureColor.z >> 16) & 255);
    vWinter = allSeason ? 0.0 : mix(previous == 0 ? 1.0 : 0.0, following == 0 ? 1.0 : 0.0, baseFade);
    int tile = aTextureColor.x;
    bool door = kind == 12 && (aTextureColor.w & 256) != 0;
    if (door) {
        uint observed = uint(uTerrainDoors.x);
        int direction = (aTextureColor.w >> 18) & 3;
        int state = 0;
        if (observed != 0u && uint(aTextureColor.y) == observed)
            state = (uTerrainDoors.y >> (direction*2)) & 3;
        else if (observed != 0u && uint(aTextureColor.z) == observed)
            state = (uTerrainDoors.y >> ((direction^1)*2)) & 3;
        vWallStyle = (vWallStyle & ~1024) | (state != 0 ? 1024 : 0);
        tile += state == 2 ? 6 : 0;
    }
    vTiles = ivec2(tile + previous * 6, tile + following * 6);
    vTreeUV = uv;
    if ((kind == 10 || kind == 11) && ((aTextureColor.w >> 8) & 1) != 0) vTreeUV.x = 1.0 - vTreeUV.x;
    vSeasonMix = fade;
    vColor = vec4(uNamedColors[kind == 13 || door ? 0 : aTextureColor.y].rgb, 1.0);
    if (uTerrainShadow.w > 0.5) {
        // Permanent shade/dark rooms do not receive direct-light shadows.
        vColor.a = aTextureColor.y == 0 && castsShadow && (aTextureColor.w & 8192) == 0 ? 1.0 : 0.0;
    }
}
