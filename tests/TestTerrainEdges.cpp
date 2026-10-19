// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 The MMapper Authors

#include "display/TerrainEdges.h"

#include <bit>
#include <cassert>
#include <cmath>
#include <cstdint>
#include <limits>

int main()
{
    using namespace terrain_edges;
    // Every side is independent; shader encoding survives all sector pairs.
    for (uint32_t a = 0; a <= static_cast<uint32_t>(Sector::UNDERWATER); ++a) {
        for (uint32_t b = 0; b <= static_cast<uint32_t>(Sector::UNDERWATER); ++b) {
            const auto first = static_cast<Sector>(a);
            const auto second = static_cast<Sector>(b);
            const Neighbours neighbours{first, second, first, second};
            const auto packed = pack(second, neighbours);
            assert((packed & 15u) == b);
            assert(packed < (1u << 20));
            for (uint32_t dir = 0; dir < 4; ++dir)
                assert(neighbour(packed, dir) == neighbours[dir]);
        }
    }
    assert(!hasBank(Neighbours{}));
    assert(!hasBank(Neighbours{Sector::WATER, Sector::WATER, Sector::UNKNOWN, Sector::WATER}));
    assert(hasBank(Sector::MARSH)); // Shallows are marsh, not open water.
    assert(hasBank(Sector::MOUNTAINS));
    assert(hasBank(Sector::URBAN));
    assert(hasBank(Neighbours{Sector::WATER, Sector::MARSH, Sector::WATER, Sector::UNKNOWN}));

    // Identical joining widths from both sides, including road -> trail.
    static_assert(std::bit_cast<uint32_t>(joinHalfWidth(true, Sector::ROAD))
                  == std::bit_cast<uint32_t>(halfWidth(true)));
    static_assert(std::bit_cast<uint32_t>(joinHalfWidth(false, Sector::FIELD))
                  == std::bit_cast<uint32_t>(halfWidth(false)));
    static_assert(std::bit_cast<uint32_t>(joinHalfWidth(true, Sector::FIELD))
                  == std::bit_cast<uint32_t>(joinHalfWidth(false, Sector::ROAD)));
    static_assert(std::bit_cast<uint32_t>(joinHalfWidth(false, Sector::UNKNOWN))
                  == std::bit_cast<uint32_t>(halfWidth(false)));
    for (int kind : {ROAD, TRAIL}) {
        for (uint32_t mask = 0; mask < 16; ++mask) {
            const auto value = flags(kind, mask);
            assert((value & 15) == kind);
            assert(((value >> 4) & 15) == static_cast<int>(mask));
        }
    }
    assert(BANK != ROAD && ROAD != TRAIL && TRAIL != ROAD_BASE);
    assert(seed(0) == seed(0));
    assert(seed(std::numeric_limits<uint32_t>::max()) <= 65535u);
    bool varied = false;
    for (uint32_t id = 1; id < 10000; ++id) {
        assert(seed(id) <= 65535u);
        varied |= seed(id) != seed(id - 1);
    }
    assert(varied);
}
