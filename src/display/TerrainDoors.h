#pragma once
// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 The MMapper Authors

#include "TerrainSprites.h"

#include <cstdint>

namespace terrain_doors {
// Static variation is symmetric so paired room-owned halves match.
constexpr uint32_t variant(const uint32_t room, const uint32_t other, const unsigned axis)
{
    const auto low = other != 0 && other < room ? other : room;
    const auto high = other > room ? other : room;
    return terrain_sprites::mix(low ^ terrain_sprites::mix(high ^ (axis * 0x9e3779b9u))) % 6u;
}
} // namespace terrain_doors
