// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 The MMapper Authors
uniform mat4 uMVP;
uniform vec4 uTerrainShadow;
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
void main()
{
    const vec2 corners[4] = vec2[4](vec2(0, 0), vec2(1, 0), vec2(0, 1), vec2(1, 1));
    vec2 uv = corners[gl_VertexID];
    int kind = aTextureColor.w & 15;
    vKind = kind;
    // All six images share the trunk anchor at (0.5, 0.06) from bottom-left.
    vec2 p = aAnchorSizeLayer.xy + (uv - vec2(0.5, 0.06)) * aAnchorSizeLayer.z;
    if (kind >= 2) {
        vec2 offset = uv - vec2(0.5);
        int rotation = (aTextureColor.w >> 4) & 3;
        for (int i = 0; i < rotation; ++i) offset = vec2(-offset.y, offset.x);
        p = aAnchorSizeLayer.xy + offset * aAnchorSizeLayer.z;
    }
    if (uTerrainShadow.w > 0.5 && kind == 0) {
        vec2 direction = uTerrainShadow.xy;
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
        baseFade = smoothstep(0.0, 1.0, clamp(progress, 0.0, 1.0));
        following = int(mod(boundary, 4.0));
        previous = (following + 3) % 4;
        float start = float(aTextureColor.z) / 65535.0 * 24.0;
        fade = clamp((progress * 28.0 - start) / 4.0, 0.0, 1.0);
    }
    const vec3 groundColors[4] = vec3[4](vec3(0.89, 0.91, 0.93),
        vec3(0.35, 0.42, 0.18), vec3(0.36, 0.40, 0.18), vec3(0.50, 0.43, 0.24));
    vBaseColor = mix(groundColors[previous], groundColors[following], baseFade);
    vTiles = ivec2(aTextureColor.x + previous * 6, aTextureColor.x + following * 6);
    vTreeUV = uv;
    vSeasonMix = kind == 2 ? baseFade : fade;
    vColor = vec4(uNamedColors[aTextureColor.y].rgb, 1.0);
    if (uTerrainShadow.w > 0.5) {
        // Permanent shade/dark rooms do not receive direct-light shadows.
        vColor.a = aTextureColor.y == 0 && kind == 0 ? 1.0 : 0.0;
    }
}
