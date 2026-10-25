#pragma once
// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 The MMapper Authors

#include <array>
#include <cstddef>

namespace terrain_materials {
// Shared sheets follow the regional tree sheets in one texture array. Every
// six-column/four-row sheet occupies two consecutive 4x4 atlas layers.
// Seasonal materials and fixed wall-finishing roles share that stable stride.
enum class Family : int {
    FOREST_GROUND,
    FIELD_GROUND,
    FIELD_DETAIL,
    MOUNTAIN_GROUND,
    MOUNTAIN_PEAK,
    HILL_GROUND,
    HILL,
    MARSH_GROUND,
    REEDS,
    BRUSH_GROUND,
    BRUSH,
    ROAD,
    WATER,
    BORDER,
    TUNNEL_GROUND,
    CAVERN_GROUND,
    CAVE_DETAIL,
    TUNNEL_WALL,
    CAVERN_WALL,
    CITY_GROUND,
    INDOOR_GROUND,
    TIMBER_WALL,
    CITY_EAVE,
    STONE_WALL,
    WALL_FINISH,
    UNDERWATER_GROUND,
    UNDERWATER_DETAIL,
    DOOR,
    DEATH_TRAP,
    ROAD_SHORE,
    RAPIDS,
    WATER_FLOW
};
enum class Placement {
    PATCH,
    SOFT_PATCH,
    UPRIGHT,
    MATERIAL,
    BORDER,
    WALL,
    REPEAT,
    EAVE,
    MASONRY,
    FINISH
};
struct Sheet final
{
    const char *filename;
    Placement placement;
};
inline constexpr int LAYERS_PER_SHEET = 2;
inline constexpr int TILES_PER_SHEET = 32;
inline constexpr std::array<Sheet, 32> SHEETS{
    {{"terrain-forest-ground-seasons.png", Placement::PATCH},
     {"terrain-field-ground-seasons.png", Placement::PATCH},
     {"terrain-field-details-seasons.png", Placement::UPRIGHT},
     {"terrain-mountain-ground-seasons.png", Placement::PATCH},
     {"terrain-mountain-peaks-seasons.png", Placement::UPRIGHT},
     {"terrain-hill-ground-seasons.png", Placement::PATCH},
     {"terrain-hills-seasons.png", Placement::UPRIGHT},
     {"terrain-marsh-ground-seasons.png", Placement::PATCH},
     {"terrain-marsh-reeds-seasons.png", Placement::UPRIGHT},
     {"terrain-brush-ground-seasons.png", Placement::PATCH},
     {"terrain-brush-seasons.png", Placement::UPRIGHT},
     {"terrain-road-seasons.png", Placement::MATERIAL},
     {"terrain-water-patches-seasons.png", Placement::SOFT_PATCH},
     {"terrain-borders-seasons.png", Placement::BORDER},
     {"terrain-tunnel-ground.png", Placement::PATCH},
     {"terrain-cavern-ground.png", Placement::PATCH},
     {"terrain-cave-details.png", Placement::UPRIGHT},
     {"terrain-tunnel-walls.png", Placement::WALL},
     {"terrain-cavern-walls.png", Placement::WALL},
     {"terrain-city-ground-v2.png", Placement::REPEAT},
     {"terrain-indoor-ground-v2.png", Placement::REPEAT},
     {"terrain-timber-walls.png", Placement::WALL},
     {"terrain-city-eaves-v3.png", Placement::EAVE},
     {"terrain-stone-walls.png", Placement::MASONRY},
     {"terrain-wall-finishes.png", Placement::FINISH},
     {"terrain-underwater-ground.png", Placement::PATCH},
     {"terrain-underwater-details.png", Placement::UPRIGHT},
     {"terrain-doors.png", Placement::MATERIAL},
     {"terrain-death-trap.png", Placement::MATERIAL},
     {"terrain-road-shores.png", Placement::MATERIAL},
     {"terrain-rapids-patches.png", Placement::SOFT_PATCH},
     {"terrain-water-flow-ribbon.png", Placement::MATERIAL}}};
constexpr int atlas(const int firstShared, const Family family)
{
    return firstShared + LAYERS_PER_SHEET * static_cast<int>(family);
}
inline constexpr int ALL_SEASON = 1 << 12; // Use all 24 atlas cells as fixed variants.
inline constexpr int NO_SKY_SHADOW = 1 << 13;
inline constexpr int CASTS_SHADOW = 1 << 14; // Upright foreground only.
inline constexpr int LOW_SHADOW = 1 << 15;
inline constexpr int MID_SHADOW = 1 << 16;
inline constexpr int DOOR_OVERLAY = 1 << 8; // Kind 12 has its own role flags.
inline constexpr int DOOR_OTHER_SIDE = 1 << 9;
inline constexpr int DOOR_PAIRED = 1 << 10; // A reciprocal, adjacent saved-map door.
inline constexpr int ROUTE_APRON = 1 << 15; // Route kinds only.
inline constexpr int WARNING_KIND = 10;
inline constexpr int WALL_KIND = 12;
inline constexpr int WALL_SHADOW_KIND = 13;
inline constexpr int GRIME_KIND = 14;
inline constexpr int PAVING_KIND = 15;
inline constexpr int CITY_GUTTER = 1 << 14;
inline constexpr int SHINGLE_EAVE = 1 << 17;
inline constexpr int WALL_DEPTH_SHIFT = 20;
inline constexpr int STONE_FACE = 1 << 15;
inline constexpr int TIMBER_FACE = 1 << 16;
inline constexpr int WALL_END_SHIFT = 18;
inline constexpr int WALL_CORNER_ONLY = 1 << 9;
inline constexpr int HAS_WALL_FINISH = 1 << 10;
inline constexpr int ROOF_CAP_OVERLAY = 1 << 11;
inline constexpr float STONE_WALL_DEPTH = 0.11f;
inline constexpr int FOREGROUND_KIND = 8;
inline constexpr int WATER_KIND = 9;
inline constexpr int BORDER_KIND = 10;
inline constexpr int WATER_PATCH_KIND = 11;
inline constexpr int FLOW_OVERLAY = 1 << 18;   // Kind 10, independent of hazard artwork.
inline constexpr int FLOW_INCOMING = 1 << 19;  // Sample the second half of the same ribbon.
inline constexpr int RAPIDS_OVERLAY = 1 << 19; // Kind 11, drawn above ordinary water.
} // namespace terrain_materials
