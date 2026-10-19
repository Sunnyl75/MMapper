#pragma once
// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 The MMapper Authors

#include "../map/mmapper2room.h"
#include "TerrainMaterials.h"

#include <array>
#include <optional>

namespace terrain_walls {
// Artistic map scale: a 20-foot room and a 10-foot wall. Sky projection is
// capped by the existing lighting model at three times height (1.5 rooms).
inline constexpr float WALL_HEIGHT_FEET = 10.f;
inline constexpr float ROOM_WIDTH_FEET = 20.f;
inline constexpr float WALL_HEIGHT_ROOMS = WALL_HEIGHT_FEET / ROOM_WIDTH_FEET;
inline constexpr std::array<std::array<int, 2>, 25> shadowOffsets = [] {
    std::array<std::array<int, 2>, 25> result{};
    for (int y = -2; y <= 2; ++y)
        for (int x = -2; x <= 2; ++x)
            result[static_cast<size_t>((y + 2) * 5 + x + 2)] = {x, y};
    return result;
}();
constexpr bool isIndoors(const RoomTerrainEnum terrain)
{
    return terrain == RoomTerrainEnum::INDOORS || terrain == RoomTerrainEnum::TUNNEL
           || terrain == RoomTerrainEnum::CAVERN;
}
constexpr bool receivesSkyShadows(const RoomTerrainEnum terrain,
                                  const RoomLightEnum light,
                                  const RoomSundeathEnum sundeath)
{
    return !isIndoors(terrain) && terrain != RoomTerrainEnum::UNDERWATER
           && light != RoomLightEnum::DARK && sundeath != RoomSundeathEnum::NO_SUNDEATH;
}
constexpr std::optional<terrain_materials::Family> family(const RoomTerrainEnum terrain,
                                                          const bool acrossIndoors = false,
                                                          const bool acrossCity = false)
{
    if (terrain == RoomTerrainEnum::TUNNEL)
        return terrain_materials::Family::TUNNEL_WALL;
    if (terrain == RoomTerrainEnum::CAVERN)
        return terrain_materials::Family::CAVERN_WALL;
    if (terrain == RoomTerrainEnum::INDOORS)
        return terrain_materials::Family::TIMBER_WALL;
    if (terrain == RoomTerrainEnum::CITY)
        return acrossIndoors ? terrain_materials::Family::CITY_EAVE
                             : terrain_materials::Family::STONE_WALL;
    if (terrain != RoomTerrainEnum::UNDERWATER && (acrossIndoors || acrossCity))
        return terrain_materials::Family::STONE_WALL;
    return std::nullopt; // No generic wall substitution for other environments.
}
inline constexpr float GUTTER_DEPTH = 0.012f;
constexpr float depth(const terrain_materials::Family material)
{
    using F = terrain_materials::Family;
    return material == F::TIMBER_WALL  ? 0.07f
           : material == F::CITY_EAVE  ? 0.24f
           : material == F::STONE_WALL ? terrain_materials::STONE_WALL_DEPTH
                                       : 0.20f;
}
constexpr unsigned depthByte(const float value)
{
    return static_cast<unsigned>(value * 255.f + 0.5f);
}
// Local strip left/right after the same NESW rotation used by the shader.
constexpr std::array<unsigned, 2> alongDirections(const unsigned direction)
{
    constexpr std::array<std::array<unsigned, 2>, 4> along{{{3, 2}, {2, 3}, {0, 1}, {1, 0}}};
    return along.at(direction);
}
// Only genuine terminations get caps. A perpendicular wall is a corner,
// while a matching collinear neighbour must remain a seamless straight run.
constexpr unsigned ends(const unsigned direction, const unsigned continuationMask)
{
    const auto along = alongDirections(direction);
    return ((continuationMask & (1u << along[0])) == 0 ? 1u : 0u)
           | ((continuationMask & (1u << along[1])) == 0 ? 2u : 0u);
}
constexpr unsigned openEnds(const unsigned direction, const unsigned endMask, const unsigned walls)
{
    const auto along = alongDirections(direction);
    return endMask & ~(((walls >> along[0]) & 1u) | (((walls >> along[1]) & 1u) << 1u));
}
constexpr unsigned rotation(const unsigned direction)
{
    constexpr std::array<unsigned, 4> turns{0, 2, 3, 1}; // N,S,E,W; source edge is north.
    return turns.at(direction);
}
} // namespace terrain_walls
