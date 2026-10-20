#pragma once
// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 The MMapper Authors

#include "TerrainSprites.h"

#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>

class QJsonObject;
namespace terrain_doors {
enum class State : uint8_t { UNKNOWN, CLOSED, OPEN };
constexpr int32_t roomWord(const uint32_t room)
{
    return std::bit_cast<int32_t>(room);
}
// Only the currently observed room; never persisted into the saved map.
struct Observation final
{
    uint32_t serverRoom = 0;
    std::array<State, 4> states{};
    bool observe(const QJsonObject &object, bool partial);
    void clear() { *this = {}; }
    // Exact integer words: do not round 32-bit room IDs through a float uniform.
    std::array<int32_t, 4> renderState() const
    {
        uint32_t packed = 0;
        for (std::size_t side = 0; side < states.size(); ++side)
            packed |= static_cast<uint32_t>(states[side]) << (side * 2u);
        return {roomWord(serverRoom), static_cast<int32_t>(packed), 0, 0};
    }
    bool operator==(const Observation &) const = default;
};
constexpr uint32_t variant(const uint32_t room, const uint32_t other, const unsigned axis)
{
    const auto low = other != 0 && other < room ? other : room;
    const auto high = other > room ? other : room;
    return terrain_sprites::mix(low ^ terrain_sprites::mix(high ^ (axis * 0x9e3779b9u))) % 6u;
}
} // namespace terrain_doors
