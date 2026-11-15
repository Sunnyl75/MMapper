#pragma once
// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 The MMapper Authors

#include "TerrainSprites.h"

#include <algorithm>
#include <array>
#include <cstdint>

namespace terrain_gardens {
inline constexpr std::size_t COUNT = 12;
// Clearances are physical neighbouring wall depths, or doorway approach space.
// Every image remains a square quad, wholly inside its owning room.
inline std::array<terrain_sprites::Sprite, COUNT> sprites(const uint32_t roomSeed,
                                                          const unsigned side,
                                                          const float wallDepth,
                                                          const std::array<float, 4> &clearances)
{
    const unsigned first = side < 2 ? 3u : 1u;
    const unsigned last = side < 2 ? 2u : 0u;
    constexpr float margin = 0.12f;
    const float lo = std::min(0.5f, clearances[first] + margin);
    const float hi = std::max(0.5f, 1.f - clearances[last] - margin);
    const float step = (hi - lo) / static_cast<float>(COUNT);
    std::array<terrain_sprites::Sprite, COUNT> result{};
    for (std::size_t i = 0; i < COUNT; ++i) {
        const auto h = terrain_sprites::mix(roomSeed ^ (0x47415244u + side * 977u)
                                            ^ (static_cast<uint32_t>(i) * 65537u));
        const float size = 0.18f + static_cast<float>(h & 255u) / 255.f * 0.04f;
        const float offset = (static_cast<float>((h >> 8) & 255u) / 255.f - 0.5f);
        const float along = lo + (static_cast<float>(i) + 0.5f + offset * 0.6f) * step;
        const float inset = std::clamp(wallDepth + 0.045f
                                           + (static_cast<float>((h >> 16) & 255u) / 255.f - 0.5f)
                                                 * 0.035f,
                                       size * 0.5f + 0.01f,
                                       0.5f - size * 0.5f - 0.01f);
        const float x = side < 2 ? along : side == 2 ? 1.f - inset : inset;
        const float y = side >= 2 ? along : side == 0 ? 1.f - inset : inset;
        result[i] = {x, y, size, h % 6u, h & 65535u};
    }
    return result;
}
} // namespace terrain_gardens
