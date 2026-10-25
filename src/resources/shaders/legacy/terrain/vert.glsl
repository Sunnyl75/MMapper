// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 The MMapper Authors
precision highp int;
// Ground and the native room-tint pass must agree for the existing EQUAL
// depth test, including on drivers that optimise the two shaders differently.
invariant gl_Position;
uniform mat4 uMVP;
uniform vec4 uTerrainShadow;
uniform float uTerrainDay;
uniform ivec4 uTerrainDoorRoom; // Observed room x/y/z and N/S/E/W open mask.
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
flat out float vRoomLayer;
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
        bool flow = kind == 10 && (aTextureColor.w & 262144) != 0;
        // One canonical ribbon spans both rooms. The existing stream visitor
        // chooses the half; each quad ends exactly at its own room boundary.
        if (flow) offset *= vec2(0.60, 0.50);
        int rotation = (kind == 2 || kind == 12 || flow) ? (aTextureColor.w >> 4) & 3 : 0;
        for (int i = 0; i < rotation; ++i) offset = vec2(-offset.y, offset.x);
        p = aAnchorSizeLayer.xy + offset * (kind == 13 ? 1.0 : aAnchorSizeLayer.z);
    }
    if (kind == 4 && (aTextureColor.w & 32768) != 0) {
        int mask = (aTextureColor.w >> 4) & 15;
        float along = uv.x * 1.36 - 0.18;
        float inward = 0.40 - uv.y * 0.54;
        vec2 local = (mask & 1) != 0 ? vec2(along, 1.0-inward)
                   : (mask & 2) != 0 ? vec2(along, inward)
                   : (mask & 4) != 0 ? vec2(1.0-inward, along)
                                    : vec2(inward, along);
        p = floor(aAnchorSizeLayer.xy) + local;
    }
    if (uTerrainShadow.w > 0.5 && castsShadow) {
        vec2 direction = uTerrainShadow.xy;
        if (kind == 8) {
            int height = (aTextureColor.w >> 15) & 3;
            direction *= height == 1 ? 0.35 : height == 2 ? 0.55 : 1.0;
        }
        if (kind == 8 && (aTextureColor.w & 32768) == 0) {
            // Relief keeps the artwork's east/west profile. Cover the swept
            // footprint without rotating the peaks or collapsing at E/W light.
            vec2 tip = direction * 0.94;
            vec2 lo = min(vec2(0.0),tip)-vec2(0.52,0.16);
            vec2 hi = max(vec2(0.0),tip)+vec2(0.52,0.16);
            uv = mix(lo,hi,uv); // Ground coordinates for the relief shadow only.
            p = aAnchorSizeLayer.xy + aAnchorSizeLayer.z * uv;
        } else {
            vec2 across = normalize(vec2(direction.y, -direction.x));
            p = aAnchorSizeLayer.xy + aAnchorSizeLayer.z
                * (across * (uv.x - 0.5) * 0.75 + direction * (uv.y - 0.06));
        }
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
    vRoomLayer = aAnchorSizeLayer.w;
    vWallMask = (aTextureColor.z >> 24) & 15;
    vWaterClip = ivec3(ivec2(floor(aAnchorSizeLayer.xy)), (aTextureColor.z >> 16) & 255);
    vWinter = allSeason ? 0.0 : mix(previous == 0 ? 1.0 : 0.0, following == 0 ? 1.0 : 0.0, baseFade);
    // Walls carry their pack selection above the ordinary tile index.
    int tile = kind == 12 ? (aTextureColor.x & 1048575) : aTextureColor.x;
    bool door = kind == 12 && (aTextureColor.w & 256) != 0;
    vTiles = ivec2(tile + previous * 6, tile + following * 6);
    if (door && uTerrainDoorRoom.w != 0) {
        int turn = (aTextureColor.w >> 4) & 3;
        int side = turn == 0 ? 0 : turn == 2 ? 1 : turn == 3 ? 2 : 3;
        const ivec2 steps[4] = ivec2[4](ivec2(0,1),ivec2(0,-1),ivec2(1,0),ivec2(-1,0));
        ivec3 owner = ivec3(ivec2(floor(aAnchorSizeLayer.xy)),int(aAnchorSizeLayer.w));
        bool own = all(equal(owner,uTerrainDoorRoom.xyz));
        bool paired = (aTextureColor.w & 1024) != 0
            && all(equal(owner+ivec3(steps[side],0),uTerrainDoorRoom.xyz));
        int observedSide = own ? side : (side ^ 1);
        if ((own || paired) && (uTerrainDoorRoom.w & (1 << observedSide)) != 0)
            vTiles += ivec2(6); // Open frames reuse the exact closed-frame posts.
    }
    vTreeUV = uv;
    if (kind == 10 && (aTextureColor.w & 262144) != 0)
        vTreeUV.y = uv.y*0.5 + ((aTextureColor.w & 524288) != 0 ? 0.5 : 0.0);
    if ((kind == 10 || kind == 11) && ((aTextureColor.w >> 8) & 1) != 0) vTreeUV.x = 1.0 - vTreeUV.x;
    vSeasonMix = fade;
    vColor = vec4(uNamedColors[kind == 13 || door ? 0 : aTextureColor.y].rgb, 1.0);
    if (uTerrainShadow.w > 0.5) {
        // Permanent shade/dark rooms do not receive direct-light shadows.
        vColor.a = aTextureColor.y == 0 && castsShadow && (aTextureColor.w & 8192) == 0 ? 1.0 : 0.0;
    }
}
