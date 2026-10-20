// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 The MMapper Authors
precision highp int;
uniform sampler2DArray uTexture;
uniform vec4 uColor;
uniform vec4 uTerrainShadow;
in vec2 vTreeUV;
flat in ivec2 vTiles;
flat in float vSeasonMix;
in vec4 vColor;
flat in int vKind;
flat in vec3 vBaseColor;
flat in ivec3 vSurface;
flat in float vWinter;
in vec2 vWorldUV;
flat in float vRoomLayer;
flat in ivec2 vGroundSeasons;
flat in float vGroundMix;
flat in int vSharedBase;
flat in ivec3 vWaterClip;
flat in int vWallMask;
flat in uvec4 vShadowWalls;
flat in int vWallStyle;
flat in float vWallHeight;
flat in int vGroundRoads;
out vec4 vFragmentColor;
vec4 sampleTile(int tile, vec2 at) {
    vec2 uv = clamp(at, vec2(0.5 / 128.0), vec2(1.0 - 0.5 / 128.0));
    int cell = tile % 16;
    uv = (vec2(cell % 4, cell / 4) + uv) / 4.0;
    return texture(uTexture, vec3(uv, float(tile / 16)));
}
vec4 sampleTileGrad(int tile, vec2 at, vec2 dx, vec2 dy) {
    int cell = tile % 16;
    vec2 tileSize = vec2(textureSize(uTexture,0).xy)/4.0;
    float footprint = max(length(dx*tileSize),length(dy*tileSize));
    float level = ceil(max(0.0,log2(max(footprint,1.0))));
    // Each mip needs its own half-texel guard. A base-level clamp leaks the
    // neighbouring atlas cell at overview zoom, outlining roofs with stray lines.
    vec2 guard = min(vec2(0.5),vec2(exp2(level))/(2.0*tileSize));
    vec2 uv = clamp(at,guard,vec2(1.0)-guard);
    uv = (vec2(cell % 4,cell / 4)+uv)/4.0;
    return textureGrad(uTexture,vec3(uv,float(tile/16)),dx/4.0,dy/4.0);
}
vec4 repeatMaterial(int layer, vec2 world) {
    vec2 uv = world * 0.25;
    return textureGrad(uTexture,
        vec3(clamp(fract(uv), vec2(0.5/512.0), vec2(1.0-0.5/512.0)), float(layer)),
        dFdx(uv), dFdy(uv));
}
vec4 crossfade(vec4 a, vec4 b, float fade) {
    float alpha = mix(a.a, b.a, fade);
    vec3 rgb = mix(a.rgb * a.a, b.rgb * b.a, fade);
    return vec4(alpha > 0.0 ? rgb / alpha : vec3(0.0), alpha);
}
// Family indices match display/TerrainMaterials.h; each family is 32 cells.
vec4 material(int family, int variant, vec2 uv) {
    ivec2 seasons = (vSurface.z & 256) != 0 && family < 14 ? ivec2(2) : vGroundSeasons;
    int base = vSharedBase + family * 32 + variant;
    if (vGroundMix <= 0.0) return sampleTile(base + seasons.x * 6, uv);
    if (vGroundMix >= 1.0) return sampleTile(base + seasons.y * 6, uv);
    return crossfade(sampleTile(base + seasons.x * 6, uv),
                     sampleTile(base + seasons.y * 6, uv), vGroundMix);
}
vec2 continuousUV(vec2 world) {
    // Mirrored repetition meets exactly at the edge, without relying on a
    // generated image being mathematically seamless or restarting per room.
    return abs(fract(world * 0.47) * 2.0 - 1.0);
}

// Sector IDs and direction order are defined in display/TerrainEdges.h.
int here() { return vSurface.x & 15; }
int across(int dir) { return (vSurface.x >> ((dir + 1) * 4)) & 15; }
bool grassy(int sector) {
    return sector == 1 || sector == 2 || sector == 3 || sector == 5 || sector == 10;
}
vec3 sectorColor(int sector) {
    vec3 color = vec3(0.42, 0.38, 0.29);
    if (sector == 1) color = vBaseColor;
    if (sector == 2) color = vBaseColor * vec3(0.70, 0.75, 0.70);
    if (sector == 3) color = vBaseColor * vec3(0.95, 0.95, 0.90);
    if (sector == 4 || sector == 9) color = vec3(0.44, 0.43, 0.39);
    if (sector == 5) color = vec3(0.28, 0.32, 0.22);
    if (sector == 6) color = vec3(0.25, 0.34, 0.36);
    if (sector == 8) color = vec3(0.48, 0.46, 0.42);
    if (sector == 10) color = vBaseColor * vec3(0.80, 0.78, 0.70);
    if (sector == 11) return vec3(0.64,0.60,0.53);
    if (sector == 12) return vec3(0.64,0.65,0.64);
    if (sector == 14) return vec3(0.38,0.48,0.45);
    return mix(color, vec3(0.86, 0.88, 0.89), vWinter * (sector == 6 ? 0.25 : 1.0));
}

// No per-frame random generator. The layout is fixed by the saved map room ID.
float variation(float salt) {
    uint h = uint(vSurface.y) ^ (uint(salt) * 0x9e3779b9u);
    h ^= h >> 16;
    h *= 0x7feb352du;
    h ^= h >> 15;
    return float(h & 65535u) / 65535.0;
}
float edgeDistance(vec2 p, int dir) {
    if (dir == 0) return 1.0 - p.y;
    if (dir == 1) return p.y;
    if (dir == 2) return 1.0 - p.x;
    return p.x;
}
int groundFamily(int sector) {
    if (sector == 2) return 0;
    if (sector == 3) return 5;
    if (sector == 4 || sector == 9 || sector == 8) return 3;
    if (sector == 5) return 7;
    if (sector == 10) return 9;
    if (sector == 6) return 12;
    if (sector == 11) return 14;
    if (sector == 12) return 15;
    if (sector == 14) return 25;
    return 1;
}
int borderVariant(int sector) {
    if (sector == 2) return 1;
    if (sector == 4 || sector == 9 || sector == 11 || sector == 12) return 2;
    if (sector == 5 || sector == 6) return 3;
    if (sector == 10) return 4;
    if (sector == 7 || sector == 8) return 5;
    return 0;
}
vec3 groundColor(int sector, vec2 world) {
    if (sector == 8 || sector == 13)
        return repeatMaterial(vSharedBase/16 + (sector == 8 ? 19 : 20)*2, world).rgb;
    vec4 art = material(groundFamily(sector), 0, continuousUV(world));
    return mix(sectorColor(sector), art.rgb, art.a);
}
// An opaque detailed underpaint fills gaps between overlapping patches, most
// visibly where walls prevent neighbours supplying their usual soft fringe.
vec3 groundFill(int sector, vec2 world) {
    vec2 uv = mix(vec2(0.22), vec2(0.78), continuousUV(world*2.0));
    vec4 art = material(groundFamily(sector), 0, uv);
    return mix(sectorColor(sector), art.rgb, art.a);
}

vec4 bank(vec2 p) {
    float coverage = 0.0;
    vec3 color = vec3(0.0);
    float total = 0.0;
    for (int dir = 0; dir < 4; ++dir) {
        int sector = across(dir);
        // Marsh is land with a muddy/reedy bank; water-to-water has no bank.
        if (sector == 0 || sector == 6) continue;
        float along = dir < 2 ? p.x : p.y;
        // Fixed endpoints/tangents keep adjoining bank segments joined. The
        // shared baseline is independent of room seed, including at corners.
        float envelope = pow(sin(3.14159265 * along), 2.0);
        float width = 0.065 + 0.035 * envelope
            * sin(12.5663706 * along + variation(float(dir + 1)) * 6.2831853);
        float distance = edgeDistance(p, dir);
        float alpha = 1.0 - smoothstep(width - 0.012, width + 0.025, distance);
        vec3 land = groundColor(sector, vWorldUV);
        float fringe = smoothstep(width - 0.025, width + 0.005, distance);
        vec3 mud = mix(vec3(0.27, 0.25, 0.19), land, vWinter * 0.8);
        vec3 local = mix(land, mud, fringe * 0.7);
        // Sparse graphic strokes: reeds/grass at soft banks, stone chips at
        // rocky or built banks. Confined to the bank, never the open water.
        float marks = step(0.83, sin(along * 157.0 + variation(11.0) * 6.0));
        if (grassy(sector)) local *= 1.0 - 0.10 * marks * fringe;
        else local = mix(local, local * 0.72, marks * fringe * 0.35);
        color += local * alpha;
        total += alpha;
        coverage = max(coverage, alpha);
    }
    return vec4(total > 0.0 ? color / total : vec3(0.0), coverage);
}

vec4 route(vec2 p, bool road) {
    float width = road ? 0.28 : 0.075;
    float nearest = 10.0;
    int side = 0;
    for (int dir = 0; dir < 4; ++dir) {
        if ((vSurface.z & (1 << dir)) == 0) continue;
        vec2 axis = dir == 0 ? vec2(0, 1) : dir == 1 ? vec2(0, -1)
                  : dir == 2 ? vec2(1, 0) : vec2(-1, 0);
        vec2 normal = vec2(-axis.y, axis.x);
        float t = clamp(dot(p - vec2(0.5), axis) * 2.0, 0.0, 1.0);
        // Hold the last tenth straight and constant-width for identical joins.
        float joinT = min(t / 0.9, 1.0);
        float envelope = pow(sin(3.14159265 * joinT), 2.0);
        float bend = (variation(float(dir + 21)) - 0.5) * 0.065 * envelope;
        vec2 centre = vec2(0.5) + axis * (t * 0.5) + normal * bend;
        int sector = across(dir);
        float joinWidth = sector == 0 ? width
            : (width + (sector == 7 ? 0.28 : 0.075)) * 0.5;
        float localWidth = mix(width, joinWidth, joinT * joinT * (3.0 - 2.0 * joinT));
        localWidth += (variation(float(dir + 31)) - 0.5) * 0.015 * envelope;
        float distance = length(p - centre) - localWidth;
        if (distance < nearest) {
            nearest = distance;
            float crosswise = dot(p - centre, normal);
            side = dir < 2 ? (crosswise * axis.y < 0.0 ? 2 : 3)
                          : (crosswise * axis.x > 0.0 ? 0 : 1);
        }
    }
    float aa = max(fwidth(nearest), 0.002);
    float surface = 1.0 - smoothstep(-aa, aa, nearest);
    float verge = 1.0 - smoothstep(0.010, 0.045, nearest);
    int sector = road ? across(side) : here();
    if (road && (vWallMask & (1 << side)) != 0) sector = 7;
    vec3 edge = groundColor(sector, vWorldUV);
    // A wet verge remains muddy; rock/city verges have a gravel tone.
    if (sector == 5 || sector == 6) edge = mix(edge, vec3(0.28, 0.25, 0.19), 0.5);
    vec3 dirt = material(11, 0, continuousUV(vWorldUV)).rgb;
    if (!road) dirt *= 0.96;
    // At a blocked road edge, continue the road to the wall instead of
    // exposing a borrowed green verge. Open boundaries keep their fringe.
    if (road) {
        for (int dir = 0; dir < 4; ++dir) {
            if ((vWallMask & (1 << dir)) != 0)
                surface = max(surface, 1.0-smoothstep(0.48,0.52,edgeDistance(p,dir)));
        }
    }
    vec4 result = vec4(mix(edge, dirt, surface), max(surface, verge * 0.85));
    // Illustrated roots, stones and grass are sampled along the verge. The
    // analytical corridor only clips the artwork into the connected shape.
    float along = side < 2 ? vWorldUV.x : vWorldUV.y;
    vec4 detail = material(13, borderVariant(sector),
        vec2(fract(along * 3.0), 0.5 + nearest / 0.22));
    float amount = detail.a * (1.0 - surface) * verge;
    result.rgb = mix(result.rgb, detail.rgb, amount);
    result.a = max(result.a, amount);
    if ((vWallStyle & 32768) != 0) {
        float coverage = 0.0;
        for (int dir = 0; dir < 4; ++dir) {
            if ((vSurface.z & (1 << dir)) == 0) continue;
            float along = 0.5-edgeDistance(p,dir);
            coverage = max(coverage,smoothstep(0.04,0.20,along));
        }
        result.a *= coverage;
    }
    return result;
}
vec4 sampleTree(int tile)
{
    return sampleTile(tile, vTreeUV);
}
// Signed distance to the wall's swept footprint, including its physical
// width and rounded end caps. Taking the union BEFORE softening avoids gaps
// and darker seams at corners, T junctions and duplicate dividing walls.
float segmentDistance(vec2 p, vec2 a, vec2 b) {
    vec2 edge = b-a;
    return length(p-a-edge*clamp(dot(p-a,edge)/max(dot(edge,edge),0.000001),0.0,1.0));
}
float cross2(vec2 a, vec2 b) { return a.x*b.y-a.y*b.x; }
float sweptWallDistance(vec2 p, vec2 a, vec2 b, vec2 projection) {
    vec2 c = b+projection, d = a+projection;
    float distance = min(min(segmentDistance(p,a,b),segmentDistance(p,b,c)),
                         min(segmentDistance(p,c,d),segmentDistance(p,d,a)));
    vec4 sides = vec4(cross2(b-a,p-a),cross2(c-b,p-b),cross2(d-c,p-c),cross2(a-d,p-d));
    bool inside = abs(cross2(b-a,projection)) > 0.000001
        && (all(greaterThanEqual(sides,vec4(0))) || all(lessThanEqual(sides,vec4(0))));
    return (inside ? -distance : distance)-0.045;
}
float wallShadow(vec2 p) {
    vec2 projection = uTerrainShadow.xy * vWallHeight;
    float nearest = 10.0;
    // A little contact definition, with a broad soft far edge as height and
    // projection increase. Distances are map units, independent of zoom.
    float softness = 0.045 + 0.035*min(length(projection),1.5);
    for (int i = 0; i < 25; ++i) {
        uint mask = (vShadowWalls[i / 8] >> uint((i % 8) * 4)) & 15u;
        if (mask == 0u) continue;
        vec2 origin = vec2(i % 5 - 2, i / 5 - 2);
        for (int dir = 0; dir < 4; ++dir) {
            if ((mask & (1u << uint(dir))) == 0u) continue;
            vec2 start = origin + (dir == 0 ? vec2(0,1) : dir == 2 ? vec2(1,0) : vec2(0));
            vec2 end = start + (dir < 2 ? vec2(1,0) : vec2(0,1));
            vec2 lower = min(min(start,end),min(start+projection,end+projection));
            vec2 upper = max(max(start,end),max(start+projection,end+projection));
            if (any(lessThan(p,lower-vec2(0.045+softness)))
                || any(greaterThan(p,upper+vec2(0.045+softness)))) continue;
            nearest = min(nearest,sweptWallDistance(p,start,end,projection));
        }
    }
    return 1.0-smoothstep(-softness,softness,nearest);
}
float wallGrime(vec2 p) {
    float strength = 0.0;
    float sum = 0.0;
    for (int dir = 0; dir < 4; ++dir) {
        if ((vWallMask & (1 << dir)) == 0) continue;
        float along = dir < 2 ? vWorldUV.x : vWorldUV.y;
        float waves = 0.5 + 0.25*sin(along*19.0) + 0.25*sin(along*43.0+1.3);
        float depth = float((uint(vShadowWalls.x) >> uint(dir*8)) & 255u)/255.0;
        float width = depth + 0.14 + 0.07*waves;
        float band = 1.0 - smoothstep(max(0.0, depth-0.015), width, edgeDistance(p, dir));
        // Only free ends taper; collinear segments remain continuous and real
        // corners retain their accumulated grime. Variable taper width keeps
        // the doorway edge organic instead of exposing a room-sized cut line.
        int ends = (vWaterClip.z >> (dir*2)) & 3;
        float localAlong = dir < 2 ? p.x : p.y;
        if (dir == 1 || dir == 2) localAlong = 1.0-localAlong;
        float taper = 0.15 + 0.06*waves + 0.025*sin(edgeDistance(p,dir)*53.0+along*9.0);
        if ((ends & 1) != 0) band *= smoothstep(0.0,taper,localAlong);
        if ((ends & 2) != 0) band *= smoothstep(0.0,taper,1.0-localAlong);
        sum += band;
        strength = max(strength, band);
    }
    // Extra accumulation where two real wall bands meet; no invented corner
    // grime at open exits. Continuous world noise avoids per-room seams.
    float mottle = 0.92 + 0.08*sin(vWorldUV.x*17.0)*sin(vWorldUV.y*23.0);
    return clamp((strength * 0.34 + max(0.0, sum-strength) * 0.20)*mottle, 0.0, 0.54);
}
// Finishing pieces reuse the current material, so optional/missing artwork
// cannot acquire a substitute cap. The frames rotate with the parent strip.
vec4 wallCap(vec2 p, vec2 dx, vec2 dy, float depth, bool eave, ivec2 adjacent) {
    int ends = (vWallStyle >> 18) & 3;
    bvec2 corner = bvec2((vWallMask & (1 << adjacent.x)) != 0,
                         (vWallMask & (1 << adjacent.y)) != 0);
    float inset = 1.0-p.y;
    bool left = p.x < 0.5;
    float along = left ? p.x : 1.0-p.x;
    bool end = (ends & (left ? 1 : 2)) != 0;
    bool joined = left ? corner.x : corner.y;
    bool timber = (vWallStyle & 65536) != 0;
    bool stone = (vWallStyle & 32768) != 0;
    bool finish = (vWallStyle & 1024) != 0;
    int finishBase = vSurface.y & 65535;
    int variant = (vTiles.x & 31) % 6;
    if (eave) {
        float width = joined ? depth : 0.027;
        if ((!joined && !end) || along > width || inset > depth) return vec4(0);
        // All roof pieces come from the parent's matched fixed-role sheet.
        // Each valley has one owner, the strip's left end; the perpendicular
        // strip's right end yields the complete square below.
        if (joined && !left) return vec4(0);
        int roofBase = vTiles.x - (vTiles.x & 31);
        int tile = roofBase + (joined ? 12 : 18) + variant;
        vec2 scale = vec2((left ? 1.0 : -1.0)/width,1.0/depth);
        return sampleTileGrad(tile,vec2(along/width,1.0-inset/depth),dx*scale,dy*scale);
    }
    if (!end && !joined) return vec4(0);
    if (!timber && !stone) {
        // Exposed rock uses the existing irregular rock art with a rounded
        // return, rather than adding a timber/stone post to a cave wall.
        vec2 local = vec2(along,inset)/depth;
        if (length(local-vec2(0.5)) > 0.5) return vec4(0);
        vec2 scale = vec2((left ? 0.3 : -0.3)/depth,0.20/depth);
        return sampleTileGrad(vTiles.x,vec2(0.35+local.x*0.3,1.0-local.y*0.20),dx*scale,dy*scale);
    }
    float size = timber ? 0.115 : 0.15;
    if (along > size || inset > size) return vec4(0);
    if (stone) {
        if (!finish) return vec4(0);
        vec2 scale = vec2((left ? 1.0 : -1.0)/size,1.0/size);
        return sampleTileGrad(finishBase+18+variant,
            vec2(along/size,1.0-inset/size),dx*scale,dy*scale);
    }
    vec2 local = vec2(along,inset)/size;
    float edge = min(min(local.x,1.0-local.x),min(local.y,1.0-local.y));
    vec2 scale = vec2((left ? 0.24 : -0.24)/size,0.20/size);
    vec4 art = sampleTileGrad(vTiles.x,vec2(0.38+local.x*0.24,1.0-local.y*0.20),dx*scale,dy*scale);
    float bevel = mix(0.40,1.05,smoothstep(0.01,0.16,edge));
    float rings = timber ? 0.95+0.05*sin(length((local-0.48)*vec2(1,1.2))*64.0) : 1.0;
    return vec4(art.rgb*bevel*rings,art.a);
}
vec4 gutter(float distance, float width) {
    float t = clamp(distance/width,0.0,1.0);
    // Both edges feather; this is a shallow channel rather than an opaque
    // rectangular frame drawn around the sprite's bounding box.
    return vec4(vec3(0.22,0.20,0.16),0.48*sin(t*3.14159265));
}
float treeShadowAlpha(int tile, vec2 at) {
    float radius = 0.008+0.014*clamp(at.y,0.0,1.0);
    float alpha = sampleTile(tile,at).a*0.4;
    alpha += sampleTile(tile,at+vec2(radius,radius)).a*0.15;
    alpha += sampleTile(tile,at+vec2(-radius,radius)).a*0.15;
    alpha += sampleTile(tile,at+vec2(radius,-radius)).a*0.15;
    alpha += sampleTile(tile,at-vec2(radius,radius)).a*0.15;
    return alpha;
}
vec4 litTerrainColor()
{
    float light = roomIllumination(vec3(vWorldUV,vRoomLayer)).a;
    return vec4(mix(vColor.rgb,vec3(1.0),light),vColor.a);
}
void main()
{
    // Derivatives must be taken before per-pixel cap/mitre branches. Implicit
    // derivatives beside an early return otherwise pick different mip levels
    // when the same post is rotated ninety degrees.
    vec2 textureDx = dFdx(vTreeUV), textureDy = dFdy(vTreeUV);
    vec2 worldDx = dFdx(vWorldUV), worldDy = dFdy(vWorldUV);
    if (vKind == 13) {
        vFragmentColor = vec4(0.025, 0.035, 0.05, wallShadow(vTreeUV) * uTerrainShadow.z * uColor.a);
        return;
    }
    if (vKind == 14) {
        vFragmentColor = vec4(0.15, 0.13, 0.10, wallGrime(vTreeUV) * uColor.a);
        return;
    }
    if (vKind == 15) {
        vFragmentColor = repeatMaterial(vTiles.x/16, vWorldUV) * litTerrainColor() * uColor;
        return;
    }

    bool castsShadow = vKind == 0 || (vKind == 8 && (vWallStyle & 16384) != 0);
    bool background = vKind == 2 || vKind == 3 || vKind == 7 || vKind == 9 || vKind == 11;
    if (background || (uTerrainShadow.w > 0.5 && castsShadow)) {
        vec2 local = vWorldUV - vec2(vWaterClip.xy);
        // Keep detail right up to the boundary. The wall covers the cut;
        // an inset would expose a flat stripe on either side of the wall.
        if (((vWallMask & 1) != 0 && local.y > 1.0)
            || ((vWallMask & 2) != 0 && local.y < 0.0)
            || ((vWallMask & 4) != 0 && local.x > 1.0)
            || ((vWallMask & 8) != 0 && local.x < 0.0)) discard;
    }
    if (vKind >= 4 && vKind <= 7) {
        vec4 texel;
        if (vKind == 7) {
            // ROAD owns its entire room. Neighbouring ground sprites provide
            // the only land fringe, without an invented green verge beneath.
            texel = vec4(material(11,0,continuousUV(vWorldUV)).rgb, 1.0);
        } else {
            texel = vKind == 4 ? bank(vTreeUV) : route(vTreeUV, vKind == 5);
        }
        vFragmentColor = texel * litTerrainColor() * uColor;
        return;
    }
    if (vKind == 3) {
        vFragmentColor = vec4(groundFill(here(),vWorldUV), 1.0) * litTerrainColor() * uColor;
        return;
    }
    if (vKind == 9) {
        // Quiet opaque undercoat: detail comes from overlapping sprite patches,
        // not a periodically mirrored square water texture.
        const vec3 waterColors[4] = vec3[4](vec3(0.32,0.43,0.48),
            vec3(0.29,0.43,0.47),vec3(0.25,0.40,0.43),vec3(0.29,0.39,0.44));
        vec3 water = mix(waterColors[vGroundSeasons.x], waterColors[vGroundSeasons.y], vGroundMix);
        vFragmentColor = vec4(water,1.0) * litTerrainColor() * uColor;
        return;
    }
    if (vKind == 11 || vKind == 2) {
        ivec2 offset = ivec2(floor(vWorldUV)) - vWaterClip.xy;
        if (any(greaterThan(abs(offset), ivec2(1)))) discard;
        int bit = offset.x == 0 ? (offset.y > 0 ? 0 : 1)
                : offset.y == 0 ? (offset.x > 0 ? 2 : 3)
                : offset.y > 0 ? (offset.x > 0 ? 4 : 5)
                : (offset.x > 0 ? 6 : 7);
        if (any(notEqual(offset,ivec2(0))) && (vWaterClip.z & (1 << bit)) == 0) discard;
    }
    vec2 at = vTreeUV;
    if (vKind == 12 && (vWallStyle & 256) != 0) {
        bool other = (vWallStyle & 512) != 0;
        float sign = other ? -0.5 : 0.5;
        // Both room-owned halves sample one canonical frame. Opposite rooms
        // rotate their quads 180 degrees, so reverse BOTH texture axes here.
        vec2 uv = vec2(0.5) + sign*vec2(at.x-0.5,at.y-1.0);
        vec4 art = sampleTileGrad(vTiles.x,uv,textureDx*sign,textureDy*sign);
        if ((vWallStyle & 1024) == 0) art.rgb *= 0.78; // neutral unknown-state symbol
        if (abs(at.x-0.5) > 0.28 && 1.0-at.y < 0.03125) {
            art.rgb = mix(vec3(0.02),art.rgb,art.a);
            art.a = 1.0;
        }
        vFragmentColor = art*litTerrainColor()*uColor;
        return;
    }
    if (vKind == 12) {
        float depth = float((vWallStyle >> 20) & 255)/255.0;
        if (depth == 0.0) depth = 0.20;
        float inset = 1.0-vTreeUV.y;
        float gutterDepth = (vWallStyle & 16384) != 0 ? 0.012 : 0.0;
        bool eave = (vWallStyle & 131072) != 0;
        if ((vWallStyle & 2048) != 0) {
            // Same source frame for all fragments of an outside cap. Each
            // instance is still clipped by its ordinary room-sized quad.
            vec2 q = vec2(at.x,inset);
            int fragment = vWaterClip.z & 3;
            if (fragment == 1) q.y -= 1.0;
            if (fragment == 2) q.x -= 1.0;
            const float overlap = 0.045;
            float span = depth+2.0*overlap;
            vec2 uv = vec2((q.x+overlap)/span,1.0-(q.y+overlap)/span);
            if (any(lessThan(uv,vec2(0))) || any(greaterThan(uv,vec2(1)))) discard;
            vec4 art = sampleTileGrad(vTiles.x,uv,textureDx/span,textureDy/span);
            if ((vWallStyle & 512) != 0) {
                // Preserve both strip shoulders; pull only the convex diagonal
                // back to a round footprint, including the underlying return.
                float radius = depth+0.025;
                float distance = length(max(q,vec2(0)));
                float aa = max(fwidth(distance),0.002);
                art.a *= 1.0-smoothstep(radius-aa,radius+aa,distance);
            }
            vFragmentColor = art*litTerrainColor()*uColor;
            return;
        }
        if ((vWallStyle & 512) != 0) {
            if (eave) gutterDepth = 0.0; // Decorative lip supplies the corner finish.
            if (at.x > depth+gutterDepth || inset > depth+gutterDepth) discard;
            if (eave && length(vec2(at.x,inset)) > depth) discard;
            if (at.x > depth || inset > depth) {
                // The convex return includes the continuous gutter turn.
                // Euclidean distance softens its outer corner naturally.
                float distance = length(max(vec2(at.x,inset)-vec2(depth),vec2(0)));
                vFragmentColor = gutter(distance,gutterDepth)*litTerrainColor()*uColor;
                return;
            }
            // Dedicated square artwork for this quadrant; both axes use the
            // same scale, preserving every drawn shingle and masonry block.
            vec2 uv = vec2(at.x/depth,1.0-inset/depth);
            vec4 art = sampleTileGrad(vTiles.x,uv,textureDx/depth,textureDy/depth);
            vFragmentColor = art*litTerrainColor()*uColor;
            return;
        }
        int rotation = (vWallStyle >> 4) & 3;
        const ivec2 sides[4] = ivec2[4](ivec2(3,2),ivec2(1,0),ivec2(2,3),ivec2(0,1));
        ivec2 adjacent = sides[rotation];
        if (eave && (vWallMask & (1 << adjacent.y)) != 0
            && at.x > 1.0-depth && inset < depth) discard;
        vec4 cap = wallCap(at,textureDx,textureDy,depth,eave,adjacent);
        if (cap.a > 0.0) {
            vFragmentColor = cap*litTerrainColor()*uColor;
            return;
        }
        // Clip BEFORE sampling: clamping narrow wall UVs would bleed adjacent
        // atlas strips over the room at coarse mip levels.
        if (inset > depth + gutterDepth) discard;
        if (depth < 0.15 || eave) {
            if (((vWallMask & (1 << adjacent.x)) != 0 && at.x < inset)
                || ((vWallMask & (1 << adjacent.y)) != 0 && 1.0-at.x < inset)) discard;
        }
        if (inset > depth) {
            // A recessed drainage channel, without the old repeated grid
            // strokes that read as stray rectangular outlines around the roof.
            vec4 channel = gutter(inset-depth,gutterDepth);
            int ends = (vWallStyle >> 18) & 3;
            if ((ends & 1) != 0 && (vWallMask & (1 << adjacent.x)) == 0)
                channel.a *= smoothstep(0.0,0.04,at.x);
            if ((ends & 2) != 0 && (vWallMask & (1 << adjacent.y)) == 0)
                channel.a *= smoothstep(0.0,0.04,1.0-at.x);
            vFragmentColor = channel*litTerrainColor()*uColor;
            return;
        }
        float scale = (eave ? 0.24 : 0.20)/depth;
        at.y = 1.0-inset*scale;
        textureDx.y *= scale;
        textureDy.y *= scale;
        if (eave) {
            // A strip is a 3.125:1 physical module (0.75 by 0.24 rooms),
            // not a short image stretched across a whole room. World phase
            // carries the same shingle spacing through room boundaries.
            const vec2 axes[4] = vec2[4](vec2(1,0),vec2(0,1),vec2(-1,0),vec2(0,-1));
            vec2 axis = axes[rotation];
            float along = dot(vWorldUV,axis)+(rotation >= 2 ? 1.0 : 0.0);
            at.x = fract(along/0.75);
            textureDx.x = dot(worldDx,axis)/0.75;
            textureDy.x = dot(worldDy,axis)/0.75;
        }
    }
    vec4 texel;
    if (vKind == 12) {
        texel = crossfade(sampleTileGrad(vTiles.x,at,textureDx,textureDy),
                          sampleTileGrad(vTiles.y,at,textureDx,textureDy),vSeasonMix);
        if ((vWallStyle & 131072) != 0 && 1.0-vTreeUV.y < 0.035) {
            // The original wall occupies 4/128 room widths. Back only that
            // inner cut with opaque roof wood; leave the outer tips irregular.
            // Sample actual material, so an empty sprite stays empty.
            vec4 backing = sampleTileGrad(vTiles.x,vec2(clamp(at.x,0.04,0.96),0.94),textureDx,textureDy);
            if (backing.a > 0.5) {
                texel.rgb = mix(backing.rgb,texel.rgb,texel.a);
                texel.a = 1.0;
            }
        }
    }
    else if (vSeasonMix <= 0.0) texel = sampleTileGrad(vTiles.x, at, textureDx, textureDy);
    else if (vSeasonMix >= 1.0) texel = sampleTileGrad(vTiles.y, at, textureDx, textureDy);
    else {
        vec4 before = sampleTileGrad(vTiles.x, at, textureDx, textureDy);
        vec4 after = sampleTileGrad(vTiles.y, at, textureDx, textureDy);
        // Interpolate premultiplied colours so fading foliage has no dark fringe.
        float alpha = mix(before.a, after.a, vSeasonMix);
        vec3 rgb = mix(before.rgb * before.a, after.rgb * after.a, vSeasonMix);
        texel = vec4(alpha > 0.0 ? rgb / alpha : vec3(0.0), alpha);
    }
    if (vKind == 2) {
        // The image supplies the irregular edge; keep a broad opaque centre.
        texel.a *= 1.0 - smoothstep(0.38, 0.53, length(vTreeUV - vec2(0.5)));
        vec2 local = vWorldUV-vec2(vWaterClip.xy);
        ivec2 offset = ivec2(floor(local));
        if (any(notEqual(offset,ivec2(0)))) {
            int bit = offset.x == 0 ? (offset.y > 0 ? 0 : 1)
                    : offset.y == 0 ? (offset.x > 0 ? 2 : 3)
                    : offset.y > 0 ? (offset.x > 0 ? 4 : 5)
                    : (offset.x > 0 ? 6 : 7);
            if ((vGroundRoads & (1 << bit)) != 0) {
                vec2 outside = max(abs(local-vec2(0.5))-vec2(0.5),vec2(0.0));
                float reach = 0.10+0.035*sin(dot(vWorldUV,vec2(19,13)))
                                  +0.025*sin(dot(vWorldUV,vec2(41,-29)));
                texel.a *= 1.0-smoothstep(reach-0.018,reach+0.025,max(outside.x,outside.y));
            }
        }
    }
    if (uTerrainShadow.w > 0.5) {
        if (castsShadow) {
            texel.a = mix(treeShadowAlpha(vTiles.x,at),treeShadowAlpha(vTiles.y,at),vSeasonMix);
        }
        vFragmentColor = vec4(0.025, 0.035, 0.05,
                              texel.a * uTerrainShadow.z * litTerrainColor().a * uColor.a);
    } else {
        if (castsShadow && (vWallStyle & 8192) == 0 && uTerrainShadow.z > 0.0) {
            // A shared rounded normal proxy, not extra per-sprite geometry.
            // The illustrated front faces south; crowns/summits face upward.
            // Shadow.xy is the inverse horizontal light direction / altitude.
            vec3 light = normalize(vec3(-uTerrainShadow.xy,1.0));
            float height = smoothstep(0.10,0.92,at.y);
            vec3 normal = normalize(vec3((at.x-0.5)*1.4,
                                         -0.85+0.50*height,0.60+0.50*height));
            float incidence = max(0.0,dot(normal,light));
            float strength = clamp(uTerrainShadow.z/0.26,0.0,1.0);
            // Preserve authored ink/colour with ample ambient fill. The same
            // model follows the moon, at its existing much lower intensity.
            texel.rgb *= 1.0-strength*0.42*(1.0-incidence);
        }
        vFragmentColor = texel * litTerrainColor() * uColor;
    }
}
