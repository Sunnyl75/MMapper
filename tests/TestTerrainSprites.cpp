// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 The MMapper Authors

#include "display/TerrainLighting.h"
#include "display/TerrainSprites.h"

#include <cmath>
#include <iostream>
#include <stdexcept>

static bool same(const float left, const float right)
{
    return std::abs(left - right) < 1.e-6f;
}

static void require(const bool condition)
{
    if (!condition) {
        throw std::runtime_error("Terrain sprite layout regression");
    }
}

int main()
{
    unsigned variants = 0;
    uint32_t earliest = 65535, latest = 0;
    for (uint32_t room = 0; room < 10000; ++room) {
        const auto layout = terrain_sprites::forest(room);
        const auto repeated = terrain_sprites::forest(room);
        bool differsFromNeighbour = false;
        const auto neighbour = terrain_sprites::forest(room + 1);
        for (std::size_t i = 0; i < layout.size(); ++i) {
            const auto &sprite = layout[i];
            require(same(sprite.x, repeated[i].x) && same(sprite.y, repeated[i].y)
                    && same(sprite.size, repeated[i].size) && sprite.variant == repeated[i].variant
                    && sprite.transition == repeated[i].transition);
            require(sprite.x > 0.f && sprite.x < 1.f && sprite.y > 0.f && sprite.y < 1.f);
            require(sprite.size >= 0.52f && sprite.size < 0.64f);
            require(sprite.variant < terrain_sprites::NUM_VARIANTS);
            require(sprite.transition <= 65535);
            earliest = std::min(earliest, sprite.transition);
            latest = std::max(latest, sprite.transition);
            variants |= 1u << sprite.variant;
            differsFromNeighbour |= !same(sprite.x, neighbour[i].x) || !same(sprite.y, neighbour[i].y);
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
            require(same(patches[i].x, repeat[i].x) && same(patches[i].y, repeat[i].y)
                    && same(patches[i].size, repeat[i].size)
                    && patches[i].transition == repeat[i].transition);
            require(patches[i].x > 0.f && patches[i].x < 1.f && patches[i].y > 0.f
                    && patches[i].y < 1.f);
            require(patches[i].size >= 0.95f && patches[i].size < 1.13f);
            require(patches[i].variant < 6 && detail[i].variant < 6);
            require(detail[i].size >= 0.23f && detail[i].size < 0.315f);
            independent |= !same(patches[i].x, trees[i].x) || !same(patches[i].y, trees[i].y);
            const int flags = terrain_sprites::flags(terrain_sprites::Kind::GROUND,
                                                     terrain_sprites::mix(patches[i].transition));
            require((flags & 15) == 2 && true);
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
            }
        }
    }
    require(removed > 0 && retained > 0);
    const auto morning = terrain_lighting::sun(8.0, 6.0, 18.0);
    const auto noon = terrain_lighting::sun(12.0, 6.0, 18.0);
    const auto evening = terrain_lighting::sun(16.0, 6.0, 18.0);
    require(morning.x < 0.f && evening.x > 0.f);
    require(std::abs(noon.x) < 1.e-5f && noon.y > 0.f);
    require(std::hypot(morning.x, morning.y) > std::hypot(noon.x, noon.y));
    require(terrain_lighting::sun(6.0, 6.0, 18.0).opacity == 0.f);
    require(terrain_lighting::sun(18.0, 6.0, 18.0).opacity == 0.f);
    require(terrain_lighting::sun(0.0, 6.0, 18.0).opacity == 0.f);
    const auto summer = terrain_lighting::sun(13.0, 4.0, 22.0);
    const auto winter = terrain_lighting::sun(13.0, 9.0, 17.0);
    require(std::hypot(winter.x, winter.y) > std::hypot(summer.x, summer.y));
    const auto moon = terrain_lighting::calculate(0.0, 6.0, 18.0, 0.0, 1.0, true);
    require(moon.opacity > 0.f && moon.opacity < noon.opacity);
    require(std::abs(moon.x) < 1.e-5f && moon.y > 0.f);
    require(terrain_lighting::calculate(0.0, 6.0, 18.0, 0.0, 0.0, true).opacity == 0.f);
    require(terrain_lighting::calculate(0.0, 6.0, 18.0, 12.0, 1.0, true).opacity == 0.f);
    require(terrain_lighting::calculate(0.0, 6.0, 18.0, 0.0, 1.0, false).opacity == 0.f);
    require(terrain_lighting::calculate(18.0, 6.0, 18.0, 18.0, 1.0, true).opacity == 0.f);
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
