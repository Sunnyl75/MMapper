// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 The MMapper Authors
// Data profile order matches TerrainMaterialMaps.h. No new drawing pass.
bool terrainReflectionsEnabled() {
    return uReflectionAtlas.z > 0 && uReflectionAtlas.x >= 0 && uReflectionAtlas.y >= 0
        && uReflectionColor.a > 0.0 && uReflectionLight.w > 0.0
        && (vWallStyle & 8192) == 0 && uTerrainShadow.w <= 0.5;
}
int reflectionSource(int profile) {
    return profile == 0 ? uReflectionAtlas.x : uReflectionMaterials[profile-1];
}
int reflectionProfile(int tile) {
    if (uReflectionAtlas.w > 0)
        return int(round(packRecord(ivec2((tile/16)%512,64+(tile/16)/512)).b*255.0))-1;
    for (int profile = 0; profile < 4; ++profile) {
        int source = reflectionSource(profile);
        if (source >= 0 && tile >= source && tile < source+24) return profile;
    }
    return -1;
}
float reflectionBand(vec2 world) {
    vec2 direction = uReflectionLight.xy;
    if (length(direction) < 0.0001) return 1.0;
    vec2 across = normalize(vec2(direction.y,-direction.x));
    vec2 cell = floor(world-vec2(0.5));
    vec2 blend = smoothstep(vec2(0.0),vec2(1.0),fract(world-vec2(0.5)));
    float result = 0.0;
    for (int y = 0; y < 2; ++y)
        for (int x = 0; x < 2; ++x) {
            vec2 centre = cell+vec2(0.5)+vec2(x,y);
            float weight = (x == 0 ? 1.0-blend.x : blend.x)
                         * (y == 0 ? 1.0-blend.y : blend.y);
            result += weight*(1.0-smoothstep(0.065,0.31,abs(dot(world-centre,across))));
        }
    return result;
}
vec4 reflectionData(int profile, int colourTile, vec2 uv, vec2 dx, vec2 dy) {
    if (uReflectionAtlas.w > 0) {
        int base = recordBase(packRecord(ivec2((colourTile/16)%512,64+(colourTile/16)/512)));
        return sampleTileGrad(base+colourTile%32,uv,dx,dy);
    }
    int offset = profile*32 + colourTile-reflectionSource(profile);
    return sampleTileGrad(uReflectionAtlas.y+offset,uv,dx,dy);
}
vec4 waterMaterialData(int relativeTile, vec2 uv, vec2 dx, vec2 dy) {
    return reflectionData(0,uReflectionAtlas.x+relativeTile,uv,dx,dy);
}
vec4 seasonalReflectionData(int profile, int source, int variant, vec2 uv, vec2 dx, vec2 dy) {
    if (vGroundMix <= 0.0)
        return reflectionData(profile,source+vGroundSeasons.x*6+variant,uv,dx,dy);
    if (vGroundMix >= 1.0)
        return reflectionData(profile,source+vGroundSeasons.y*6+variant,uv,dx,dy);
    // Data alpha is reflectivity, not coverage: interpolate it directly.
    return mix(reflectionData(profile,source+vGroundSeasons.x*6+variant,uv,dx,dy),
               reflectionData(profile,source+vGroundSeasons.y*6+variant,uv,dx,dy),vGroundMix);
}
vec4 seasonalWaterData(int source, int variant, vec2 uv, vec2 dx, vec2 dy) {
    return seasonalReflectionData(0,source,variant,uv,dx,dy);
}
vec3 reflectionToLinear(vec3 c) {
    return mix(c/12.92,pow((c+0.055)/1.055,vec3(2.4)),step(vec3(0.04045),c));
}
vec3 reflectionToSrgb(vec3 c) {
    return mix(c*12.92,1.055*pow(max(c,vec3(0.0)),vec3(1.0/2.4))-0.055,
               step(vec3(0.0031308),c));
}
vec3 reflectSurface(vec3 color, vec4 data, uint turn, bool flip, int profile) {
    float response = 1.0;
    vec3 reference = vec3(0,0,1);
    float incidence = 1.0;
    if (profile == 3) {
        // Match the existing upright mountain proxy. Snow on the unlit front
        // must not acquire a ground-water stripe or shine through its shadow.
        float height = smoothstep(0.10,0.92,vTreeUV.y);
        reference = normalize(vec3((vTreeUV.x-0.5)*1.4,
                                  -0.85+0.50*height,0.60+0.50*height));
        incidence = smoothstep(0.0,0.55,dot(reference,uReflectionLight.xyz));
    }
    if (uReflectionAtlas.z == 2) {
        vec3 normal = normalize(data.rgb*2.0-1.0);
        if (flip) normal.x = -normal.x;
        normal.xy = surfaceTurn(normal.xy,(4u-turn)&3u);
        normal = normalize(reference + normal - vec3(0,0,1));
        vec3 halfway = normalize(uReflectionLight.xyz+vec3(0,0,1));
        float facing = pow(max(dot(normal,halfway),0.0),8.0);
        float flatFacing = pow(max(dot(reference,halfway),0.0),8.0);
        response = clamp((0.2+facing)/(0.2+flatFacing),0.3,2.0);
    }
    float envelope = profile == 3 ? incidence*0.60 : reflectionBand(vWorldUV);
    float weight = min(0.7,0.7*uReflectionColor.a*uReflectionLight.w
                        *data.a*envelope*response);
    vec3 linearColor = reflectionToLinear(color);
    return reflectionToSrgb(1.0-(1.0-linearColor)*(1.0-uReflectionColor.rgb*weight));
}
vec3 reflectWater(vec3 color, vec4 data, uint turn, bool flip) {
    return reflectSurface(color,data,turn,flip,0);
}
