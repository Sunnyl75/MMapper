// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 The MMapper Authors

#include "display/TerrainLighting.h"
#include "display/TerrainSprites.h"

#include <bit>
#include <iostream>
#include <stdexcept>

static void require(const bool condition)
{
    if (!condition) {
        throw std::runtime_error("Terrain sprite layout regression");
    }
}

// These checks require reproducible float bits, rather than approximate geometry.
static bool sameFloat(const float a, const float b)
{
    return std::bit_cast<uint32_t>(a) == std::bit_cast<uint32_t>(b);
}

template<typename Layout>
static uint64_t checkpointFingerprint(const Layout layout)
{
    uint64_t hash = 14695981039346656037ull;
    const auto word = [&](const uint32_t value) {
        for (unsigned byte = 0; byte < 4; ++byte) {
            hash ^= (value >> (8u * byte)) & 255u;
            hash *= 1099511628211ull;
        }
    };
    for (uint32_t sample = 0; sample < 4099; ++sample) {
        const uint32_t id = sample < 4096    ? sample
                            : sample == 4096 ? 0xffffffffu
                            : sample == 4097 ? 0x80000000u
                                             : 0x7fffffffu;
        for (const auto &sprite : layout(id)) {
            word(std::bit_cast<uint32_t>(sprite.x));
            word(std::bit_cast<uint32_t>(sprite.y));
            word(std::bit_cast<uint32_t>(sprite.size));
            word(sprite.variant);
            word(sprite.transition);
        }
    }
    return hash;
}

int main()
{
    // Frozen from the actual bdd0b7b6 header, not the replacement algorithm:
    // preserve its saved-ID scatter while adding wall fitting and water.
    require(checkpointFingerprint(terrain_sprites::forest) == 0xd0c5df2be1134f32ull);
    require(checkpointFingerprint(terrain_sprites::ground) == 0x18f6cc9d1440322dull);
    require(checkpointFingerprint(terrain_sprites::field) == 0x64465ed82d347638ull);
    unsigned variants = 0;
    uint32_t earliest = 65535, latest = 0;
    for (uint32_t room = 0; room < 10000; ++room) {
        const auto layout = terrain_sprites::forest(room);
        const auto repeated = terrain_sprites::forest(room);
        bool differsFromNeighbour = false;
        const auto neighbour = terrain_sprites::forest(room + 1);
        for (std::size_t i = 0; i < layout.size(); ++i) {
            const auto &sprite = layout[i];
            require(sameFloat(sprite.x, repeated[i].x) && sameFloat(sprite.y, repeated[i].y)
                    && sameFloat(sprite.size, repeated[i].size)
                    && sprite.variant == repeated[i].variant
                    && sprite.transition == repeated[i].transition);
            require(sprite.x > 0.f && sprite.x < 1.f && sprite.y > 0.f && sprite.y < 1.f);
            require(sprite.size >= 0.52f && sprite.size < 0.64f);
            require(sprite.variant < terrain_sprites::NUM_VARIANTS);
            require(sprite.transition <= 65535);
            earliest = std::min(earliest, sprite.transition);
            latest = std::max(latest, sprite.transition);
            variants |= 1u << sprite.variant;
            differsFromNeighbour |= !sameFloat(sprite.x, neighbour[i].x)
                                    || !sameFloat(sprite.y, neighbour[i].y);
        }
        require(differsFromNeighbour);
    }
    require(variants == (1u << terrain_sprites::NUM_VARIANTS) - 1u);
    require(earliest < 100 && latest > 65400);
    for (uint32_t room = 0; room < 2000; ++room) {
        const auto patches = terrain_sprites::ground(room);
        const auto repeat = terrain_sprites::ground(room);
        const auto detail = terrain_sprites::field(room);
        const auto trees = terrain_sprites::forest(room);
        bool independent = false;
        for (std::size_t i = 0; i < patches.size(); ++i) {
            require(sameFloat(patches[i].x, repeat[i].x) && sameFloat(patches[i].y, repeat[i].y)
                    && sameFloat(patches[i].size, repeat[i].size)
                    && patches[i].transition == repeat[i].transition);
            require(patches[i].x > 0.f && patches[i].x < 1.f && patches[i].y > 0.f
                    && patches[i].y < 1.f);
            require(patches[i].size >= 0.95f && patches[i].size < 1.13f);
            require(patches[i].variant < 6 && detail[i].variant < 6);
            require(detail[i].size >= 0.23f && detail[i].size < 0.315f);
            independent |= !sameFloat(patches[i].x, trees[i].x)
                           || !sameFloat(patches[i].y, trees[i].y);
            const auto turns = terrain_sprites::mix(patches[i].transition);
            const int flags = terrain_sprites::flags(terrain_sprites::Kind::GROUND, turns);
            require((flags & 15) == 2);
            require((flags >> 4) == static_cast<int>(turns & 3u));
        }
        require(independent);
    }
    const auto extreme = terrain_sprites::forest(UINT32_MAX);
    require(extreme.front().variant < terrain_sprites::NUM_VARIANTS);
    using terrain_sprites::TrailSegments;
    const TrailSegments northSouth{true, true, false, false};
    const TrailSegments eastWest{false, false, true, true};
    const TrailSegments northEast{true, false, true, false};
    const TrailSegments cross{true, true, true, true};
    const auto blocked = [](const float x, const float y, const TrailSegments &trail) {
        return terrain_sprites::blocksTree(terrain_sprites::Sprite{x, y, 0.6f, 0}, trail);
    };
    require(blocked(0.5f, 0.05f, northSouth));
    require(blocked(0.5f, 0.95f, northSouth));
    require(!blocked(0.05f, 0.5f, northSouth));
    require(!blocked(0.95f, 0.5f, northSouth));
    require(blocked(0.69f, 0.5f, northSouth));
    require(!blocked(0.71f, 0.5f, northSouth));
    require(blocked(0.05f, 0.5f, eastWest));
    require(blocked(0.95f, 0.5f, eastWest));
    require(!blocked(0.5f, 0.05f, eastWest));
    require(!blocked(0.5f, 0.95f, eastWest));
    require(blocked(0.5f, 0.95f, northEast));
    require(blocked(0.95f, 0.5f, northEast));
    require(blocked(0.5f, 0.5f, northEast));
    require(!blocked(0.5f, 0.05f, northEast));
    require(!blocked(0.05f, 0.5f, northEast));
    require(!blocked(0.85f, 0.85f, northEast));
    require(blocked(0.5f, 0.5f, TrailSegments{true, false, false, false}));
    require(!blocked(0.5f, 0.05f, TrailSegments{true, false, false, false}));
    require(!blocked(0.05f, 0.05f, cross));
    require(!blocked(0.95f, 0.95f, cross));
    // Check every combination of arms, including all four corner orientations.
    for (unsigned mask = 0; mask < 16; ++mask) {
        const TrailSegments arms{bool(mask & 1u), bool(mask & 2u), bool(mask & 4u), bool(mask & 8u)};
        require(blocked(0.5f, 0.95f, arms) == arms.north);
        require(blocked(0.5f, 0.05f, arms) == arms.south);
        require(blocked(0.95f, 0.5f, arms) == arms.east);
        require(blocked(0.05f, 0.5f, arms) == arms.west);
        require(blocked(0.5f, 0.5f, arms) == (mask != 0));
        require(!blocked(0.05f, 0.05f, arms));
    }
    unsigned removed = 0;
    unsigned retained = 0;
    for (uint32_t room = 0; room < 10000; ++room) {
        for (const auto &sprite : terrain_sprites::forest(room)) {
            require(!terrain_sprites::blocksTree(sprite, TrailSegments{}));
            if (terrain_sprites::blocksTree(sprite, northSouth)) {
                ++removed;
            } else {
                ++retained;
                require(sprite.x < 0.305f || sprite.x > 0.695f);
            }
        }
    }
    require(removed > 0 && retained > 0);
    for (uint32_t id = 0; id < 100; ++id) {
        const auto water = terrain_sprites::water(id);
        unsigned waterVariants = 0;
        for (const auto &patch : water) {
            require(patch.x > 0.f && patch.x < 1.f && patch.y > 0.f && patch.y < 1.f);
            require(patch.size >= 1.25f && patch.size < 1.65f);
            require(patch.variant < terrain_sprites::NUM_VARIANTS);
            waterVariants |= 1u << patch.variant;
        }
        require(waterVariants == 63u);
        require(sameFloat(water.front().x, terrain_sprites::water(id).front().x));
    }
    // Every wall combination: all retained foreground quads stay whole on
    // their own side, and fitting never changes identity, size or season seed.
    unsigned moved = 0;
    for (uint32_t walls = 0; walls < 16; ++walls) {
        uint32_t allowed = (~walls) & 15u;
        if (!(walls & 5u))
            allowed |= 16u;
        if (!(walls & 9u))
            allowed |= 32u;
        if (!(walls & 6u))
            allowed |= 64u;
        if (!(walls & 10u))
            allowed |= 128u;
        for (uint32_t id = 1; id < 100; ++id)
            for (auto sprite : terrain_sprites::forest(id)) {
                const auto original = sprite;
                require(terrain_sprites::fitForeground(sprite, walls, allowed));
                require(terrain_sprites::fitsForeground(sprite, allowed));
                require(sameFloat(sprite.size, original.size) && sprite.variant == original.variant
                        && sprite.transition == original.transition);
                moved += !sameFloat(sprite.x, original.x) || !sameFloat(sprite.y, original.y);
                auto repeat = original;
                require(terrain_sprites::fitForeground(repeat, walls, allowed));
                require(sameFloat(repeat.x, sprite.x) && sameFloat(repeat.y, sprite.y));
            }
    }
    require(moved > 0);
    unsigned shrunk = 0;
    for (uint32_t walls = 0; walls < 16; ++walls) {
        uint32_t allowed = (~walls) & 15u;
        if (!(walls & 5u))
            allowed |= 16u;
        if (!(walls & 9u))
            allowed |= 32u;
        if (!(walls & 6u))
            allowed |= 64u;
        if (!(walls & 10u))
            allowed |= 128u;
        for (uint32_t id = 1; id < 100; ++id)
            for (auto sprite : terrain_sprites::forest(id)) {
                sprite.size *= 1.9f;
                const auto original = sprite;
                require(terrain_sprites::fitForegroundScaled(sprite, walls, allowed));
                require(terrain_sprites::fitsForeground(sprite, allowed));
                require(sprite.size >= 0.45f && sprite.size <= original.size);
                require(sprite.variant == original.variant
                        && sprite.transition == original.transition);
                shrunk += sprite.size < original.size;
                auto repeat = original;
                require(terrain_sprites::fitForegroundScaled(repeat, walls, allowed));
                require(sameFloat(repeat.x, sprite.x) && sameFloat(repeat.y, sprite.y)
                        && sameFloat(repeat.size, sprite.size));
            }
    }
    require(shrunk > 0);
    auto huge = terrain_sprites::Sprite{.5f, .5f, 1.4f, 0, 123};
    require(!terrain_sprites::fitForeground(huge, 15u, 0u));
    auto corner = terrain_sprites::Sprite{.95f, .95f, .6f, 2, 456};
    require(!terrain_sprites::fitsForeground(corner, 255u & ~16u));
    require(terrain_sprites::fitForeground(corner, 0u, 255u & ~16u));
    require(terrain_sprites::fitsForeground(corner, 255u & ~16u));
    const auto morning = terrain_lighting::sun(8.0, 6.0, 18.0);
    const auto noon = terrain_lighting::sun(12.0, 6.0, 18.0);
    const auto evening = terrain_lighting::sun(16.0, 6.0, 18.0);
    require(morning.x < 0.f && evening.x > 0.f);
    require(std::abs(noon.x) < 1.e-5f && noon.y > 0.f);
    require(std::hypot(morning.x, morning.y) > std::hypot(noon.x, noon.y));
    require(sameFloat(terrain_lighting::sun(6.0, 6.0, 18.0).opacity, 0.f));
    require(sameFloat(terrain_lighting::sun(18.0, 6.0, 18.0).opacity, 0.f));
    require(sameFloat(terrain_lighting::sun(0.0, 6.0, 18.0).opacity, 0.f));
    const auto summer = terrain_lighting::sun(13.0, 4.0, 22.0);
    const auto winter = terrain_lighting::sun(13.0, 9.0, 17.0);
    require(std::hypot(winter.x, winter.y) > std::hypot(summer.x, summer.y));
    const auto moon = terrain_lighting::calculate(0.0, 6.0, 18.0, 0.0, 1.0, true);
    require(moon.opacity > 0.f && moon.opacity < noon.opacity);
    require(std::abs(moon.x) < 1.e-5f && moon.y > 0.f);
    require(sameFloat(terrain_lighting::calculate(0.0, 6.0, 18.0, 0.0, 0.0, true).opacity, 0.f));
    require(sameFloat(terrain_lighting::calculate(0.0, 6.0, 18.0, 12.0, 1.0, true).opacity, 0.f));
    require(sameFloat(terrain_lighting::calculate(0.0, 6.0, 18.0, 0.0, 1.0, false).opacity, 0.f));
    require(sameFloat(terrain_lighting::calculate(18.0, 6.0, 18.0, 18.0, 1.0, true).opacity, 0.f));
    const auto moonRising = terrain_lighting::calculate(19.0, 6.0, 18.0, 0.0, 1.0, true);
    const auto moonSetting = terrain_lighting::calculate(5.0, 6.0, 18.0, 0.0, 1.0, true);
    require(moonRising.x < 0.f && moonSetting.x > 0.f);
    for (int minute = 0; minute < 1440; ++minute) {
        const auto light = terrain_lighting::calculate(minute / 60.0, 4.0, 22.0, 0.0, 1.0, true);
        require(std::isfinite(light.x) && std::isfinite(light.y) && std::isfinite(light.opacity));
        require(static_cast<double>(std::hypot(light.x, light.y))
                <= terrain_lighting::MAX_LENGTH + 1.e-5);
        require(light.opacity >= 0.f && light.opacity <= 0.26f);
    }
    std::cout
        << "Terrain placement: deterministic layouts, valid anchors, scales, variants, trail corridors and celestial shadows passed\n";
}
