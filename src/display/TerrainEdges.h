#pragma once
// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 The MMapper Authors

#include "TerrainSprites.h"

#include <array>
#include <cstdint>

// CPU/shader interface for connected surfaces. Directions are north, south,
// east, west, matching RoadIndexMaskEnum. No Qt/GL or server IDs are needed.
namespace terrain_edges {
enum class Sector : uint32_t {
    UNKNOWN = 0,
    FIELD,
    FOREST,
    HILLS,
    MOUNTAINS,
    MARSH,
    WATER,
    ROAD,
    URBAN,
    ROCK,
    BRUSH,
    TUNNEL,
    CAVERN,
    INDOORS,
    UNDERWATER
};
using Neighbours = std::array<Sector, 4>;
constexpr uint32_t borderVariant(const Sector sector)
{
    switch (sector) {
    case Sector::FOREST:
        return 1;
    case Sector::MOUNTAINS:
    case Sector::ROCK:
    case Sector::TUNNEL:
    case Sector::CAVERN:
        return 2;
    case Sector::MARSH:
    case Sector::WATER:
        return 3;
    case Sector::BRUSH:
        return 4;
    case Sector::ROAD:
    case Sector::URBAN:
        return 5;
    default:
        return 0;
    }
}

// These kinds extend TerrainSprites::Kind without changing its existing ABI.
inline constexpr int BANK = 4;
inline constexpr int ROAD = 5;
inline constexpr int TRAIL = 6;
inline constexpr int ROAD_BASE = 7;

constexpr uint32_t pack(const Sector here, const Neighbours &neighbours)
{
    uint32_t value = static_cast<uint32_t>(here);
    for (uint32_t i = 0; i < neighbours.size(); ++i)
        value |= static_cast<uint32_t>(neighbours[i]) << ((i + 1u) * 4u);
    return value;
}

// The remaining eleven bits carry the first shared atlas tile. With 44
// regional tree packs it remains below 2048, independent of shared family count.
constexpr uint32_t withMaterials(const uint32_t sectors, const uint32_t firstTile)
{
    return sectors | (firstTile << 20u);
}

constexpr Sector neighbour(const uint32_t packed, const uint32_t direction)
{
    return static_cast<Sector>((packed >> ((direction + 1u) * 4u)) & 15u);
}

constexpr bool hasBank(const Sector across)
{
    return across != Sector::UNKNOWN && across != Sector::WATER;
}

constexpr bool hasBank(const Neighbours &neighbours)
{
    for (const auto sector : neighbours)
        if (hasBank(sector))
            return true;
    return false;
}

// The same contract is used at both ends of a road/trail boundary. Unknown
// neighbours retain the local width. It does not imply a navigable connection.
constexpr float halfWidth(const bool road)
{
    return road ? 0.28f : 0.075f;
}
constexpr float joinHalfWidth(const bool road, const Sector across)
{
    return across == Sector::UNKNOWN ? halfWidth(road)
                                     : (halfWidth(road) + halfWidth(across == Sector::ROAD)) * 0.5f;
}

constexpr uint32_t seed(const uint32_t savedRoomId)
{
    return terrain_sprites::mix(savedRoomId ^ 0x45444745u) & 65535u;
}

constexpr int flags(const int kind, const uint32_t connections)
{
    return kind | (static_cast<int>(connections & 15u) << 4);
}
} // namespace terrain_edges
