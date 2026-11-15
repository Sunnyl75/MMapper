// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 The MMapper Authors

#include "display/TerrainEdges.h"
#include "display/TerrainGardens.h"

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

    // City paving is a continuation, including exits without a ROAD flag.
    // Doors/no-exit walls still block it; unrelated sectors keep the old rule.
    for (unsigned side = 0; side < 4; ++side) {
        const uint32_t bit = 1u << side;
        for (uint32_t walls = 0; walls < 16; ++walls) {
            for (uint32_t peerMask = 0; peerMask < 16; ++peerMask) {
                assert(routePeerConnection(Sector::URBAN, peerMask, side, walls)
                       == ((walls & bit) ? 0u : bit));
                assert(routePeerConnection(Sector::ROAD, peerMask, side, walls) == 0u);
                for (const auto peer : {Sector::FIELD, Sector::HILLS, Sector::INDOORS}) {
                    const bool joins = !(walls & bit) && (peerMask & (1u << (side ^ 1u)));
                    assert(routePeerConnection(peer, peerMask, side, walls) == (joins ? bit : 0u));
                }
            }
        }
    }

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
    // Only mapped roads contribute shore; either room's wall blocks it.
    const Neighbours shore{Sector::ROAD, Sector::WATER, Sector::URBAN, Sector::ROAD};
    assert(roadShoreMask(shore, 0) == 9u);
    for (uint32_t walls = 0; walls < 16; ++walls)
        assert(roadShoreMask(shore, walls) == (9u & ~walls));
    assert(roadShoreMask(Neighbours{}, 0) == 0u);
    assert((flags(BANK, 9) | ROAD_SHORE) % 16 == BANK);
    assert(BANK != ROAD && ROAD != TRAIL && TRAIL != ROAD_BASE);
    for (unsigned side = 0; side < 4; ++side) {
        assert((flowRotation(side, false) + 2u) % 4u == flowRotation(side, true));
        // Existing outgoing and opposite-side incoming flags select the same
        // physical direction; the shader samples adjoining ribbon halves.
        assert(flowRotation(side, false) == flowRotation(side ^ 1u, true));
    }
    // North enters southward; east leaves eastward (clockwise quarter turn).
    static_assert(flowRotation(0, true) == 2u);
    static_assert(flowRotation(2, false) == 3u);
    assert(seed(0) == seed(0));
    assert(seed(std::numeric_limits<uint32_t>::max()) <= 65535u);
    bool varied = false;
    for (uint32_t id = 1; id < 10000; ++id) {
        assert(seed(id) <= 65535u);
        varied |= seed(id) != seed(id - 1);
    }
    // Twelve actual square garden sprites, stable per room and overlapping.
    for (unsigned side = 0; side < 4; ++side) {
        for (uint32_t id = 0; id < 100; ++id) {
            const std::array<float, 4> clearances{0.24f, 0.24f, 0.24f, 0.24f};
            const auto plants = terrain_gardens::sprites(id, side, 0.24f, clearances);
            const auto again = terrain_gardens::sprites(id, side, 0.24f, clearances);
            assert(plants.size() == 12);
            for (std::size_t i = 0; i < plants.size(); ++i) {
                const auto &plant = plants[i];
                assert(std::bit_cast<uint32_t>(plant.x) == std::bit_cast<uint32_t>(again[i].x)
                       && std::bit_cast<uint32_t>(plant.y) == std::bit_cast<uint32_t>(again[i].y)
                       && plant.variant == again[i].variant
                       && std::bit_cast<uint32_t>(plant.size)
                              == std::bit_cast<uint32_t>(again[i].size));
                const float half = plant.size * 0.5f;
                assert(plant.x - half >= 0.f && plant.x + half <= 1.f);
                assert(plant.y - half >= 0.f && plant.y + half <= 1.f);
                assert(plant.variant < 6 && plant.size >= 0.18f && plant.size <= 0.221f);
                const float along = side < 2 ? plant.x : plant.y;
                assert(along - half > 0.24f && along + half < 0.76f);
                if (i != 0) {
                    const auto &previous = plants[i - 1];
                    const float dx = previous.x - plant.x;
                    const float dy = previous.y - plant.y;
                    const float reach = (previous.size + plant.size) * 0.5f;
                    assert(dx * dx + dy * dy < reach * reach);
                }
            }
            const auto other = terrain_gardens::sprites(id + 1, side, 0.24f, clearances);
            assert(std::bit_cast<uint32_t>(plants[0].x) != std::bit_cast<uint32_t>(other[0].x)
                   || std::bit_cast<uint32_t>(plants[0].y) != std::bit_cast<uint32_t>(other[0].y));
        }
    }
    assert(varied);
}
