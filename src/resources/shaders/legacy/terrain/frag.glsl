// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 The MMapper Authors
uniform sampler2DArray uTexture;
uniform vec4 uColor;
uniform vec4 uTerrainShadow;
in vec2 vTreeUV;
flat in ivec2 vTiles;
flat in float vSeasonMix;
in vec4 vColor;
flat in int vKind;
flat in vec3 vBaseColor;
out vec4 vFragmentColor;
vec4 sampleTree(int tile)
{
    // Clamp within a cell to keep filtering from sampling a neighbouring tree.
    vec2 uv = clamp(vTreeUV, vec2(0.5 / 128.0), vec2(1.0 - 0.5 / 128.0));
    int cell = tile % 16;
    uv = (vec2(cell % 4, cell / 4) + uv) / 4.0;
    return texture(uTexture, vec3(uv, float(tile / 16)));
}
void main()
{
    if (vKind == 3) {
        vFragmentColor = vec4(vBaseColor, 1.0) * vColor * uColor;
        return;
    }
    vec4 texel;
    if (vSeasonMix <= 0.0) texel = sampleTree(vTiles.x);
    else if (vSeasonMix >= 1.0) texel = sampleTree(vTiles.y);
    else {
        vec4 before = sampleTree(vTiles.x);
        vec4 after = sampleTree(vTiles.y);
        // Interpolate premultiplied colours so fading foliage has no dark fringe.
        float alpha = mix(before.a, after.a, vSeasonMix);
        vec3 rgb = mix(before.rgb * before.a, after.rgb * after.a, vSeasonMix);
        texel = vec4(alpha > 0.0 ? rgb / alpha : vec3(0.0), alpha);
    }
    if (vKind == 2) {
        // Broad feather softens rims; the image supplies the irregular silhouette.
        texel.a *= 1.0 - smoothstep(0.20, 0.55, length(vTreeUV - vec2(0.5)));
    }
    if (uTerrainShadow.w > 0.5) {
        vFragmentColor = vec4(0.025, 0.035, 0.05,
                              texel.a * uTerrainShadow.z * vColor.a * uColor.a);
    } else {
        vFragmentColor = texel * vColor * uColor;
    }
}
