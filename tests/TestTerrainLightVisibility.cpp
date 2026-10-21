// SPDX-License-Identifier: GPL-2.0-or-later
#include "display/TerrainLightMask.h"
#include "display/TerrainLightVisibility.h"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <iostream>
#include <optional>
#include <vector>

#include <glm/gtc/matrix_transform.hpp>

#include <QPainter>

namespace {
using terrain_room_light::LocatedLight;
using terrain_room_light::VisibilityCache;

// Independent halo-aware selection, used as a regression oracle.
std::optional<Bounds> reference(const std::vector<LocatedLight> &lights,
                                const glm::mat4 &view,
                                const int layer,
                                const std::optional<Coordinate> live)
{
    std::optional<Bounds> bounds;
    const auto include = [&](const Coordinate position) {
        if (position.z != layer)
            return;
        glm::vec2 lo{1.e20f}, hi{-1.e20f};
        bool inFront = false;
        for (int corner = 0; corner < 4; ++corner) {
            const auto clip = view
                              * glm::vec4(position.to_vec3()
                                              + glm::vec3{static_cast<float>(corner & 1) * 2.f - .5f,
                                                          static_cast<float>(corner >> 1) * 2.f
                                                              - .5f,
                                                          0.f},
                                          1.f);
            if (clip.w <= 0.f)
                continue;
            inFront = true;
            const auto point = glm::vec2{clip} / clip.w;
            lo = glm::min(lo, point);
            hi = glm::max(hi, point);
        }
        if (!inFront || lo.x > 1.f || hi.x < -1.f || lo.y > 1.f || hi.y < -1.f)
            return;
        if (!bounds)
            bounds = Bounds{position, position};
        else {
            bounds->min.x = std::min(bounds->min.x, position.x);
            bounds->min.y = std::min(bounds->min.y, position.y);
            bounds->max.x = std::max(bounds->max.x, position.x);
            bounds->max.y = std::max(bounds->max.y, position.y);
        }
    };
    for (const auto &light : lights)
        include(light.position);
    if (live)
        include(*live);
    if (bounds) {
        --bounds->min.x;
        --bounds->min.y;
        ++bounds->max.x;
        ++bounds->max.y;
    }
    return bounds;
}

QImage mask(const std::vector<LocatedLight> &lights,
            const Bounds &bounds,
            const std::optional<Coordinate> live,
            const terrain_room_light::MaskAtlas &atlas,
            const double seconds,
            const int flicker)
{
    constexpr int pixels = terrain_room_light::MaskAtlas::PIXELS;
    QImage image((bounds.max.x - bounds.min.x + 1) * pixels,
                 (bounds.max.y - bounds.min.y + 1) * pixels,
                 QImage::Format_RGBA8888);
    image.fill(0);
    const auto draw = [&](const Coordinate position, const uint32_t seed) {
        if (bounds.contains(position))
            terrain_room_light::MaskAtlas::composite(
                image,
                atlas.tile(seed, seconds, 100, flicker),
                (position.x - bounds.min.x) * pixels - terrain_room_light::MaskAtlas::HALO_OFFSET,
                (position.y - bounds.min.y) * pixels - terrain_room_light::MaskAtlas::HALO_OFFSET);
    };
    for (const auto &light : lights)
        draw(light.position, light.seed);
    if (live)
        draw(*live, 99999); // Same overwrite rule as production.
    return image;
}
} // namespace

int main()
{
    const terrain_room_light::MaskAtlas atlas(
        QImage(QStringLiteral(TERRAIN_ASSET_DIR "/terrain-light-masks.png")));
    std::vector<LocatedLight> lights;
    uint32_t seed = 1;
    for (int z = 0; z < 2; ++z)
        for (int y = -8; y <= 8; y += 2)
            for (int x = -10; x <= 10; x += 2)
                lights.push_back({Coordinate{x, y, z}, seed++});
    // Duplicate coordinates must keep the original ordering/last overwrite.
    lights.push_back({Coordinate{0, 0, 0}, 800});
    VisibilityCache cache;
    cache.setLights(lights);
    const auto check =
        [&](const glm::mat4 &view, const int layer, const std::optional<Coordinate> live) {
            if (!cache.matchesView(view, layer))
                cache.updateView(view, layer);
            assert(cache.matchesView(view, layer));
            const auto expected = reference(lights, view, layer, live);
            const auto actual = cache.boundsWithLive(live);
            assert(actual == expected);
            if (!actual)
                return;
            std::vector<LocatedLight> selected;
            for (const auto &light : lights)
                if (actual->contains(light.position))
                    selected.push_back(light);
            assert(cache.maskLights(*actual) == selected);
            for (const int flicker : {0, 100})
                for (const double seconds : {0., 0.084, 0.42})
                    assert(mask(cache.maskLights(*actual), *actual, live, atlas, seconds, flicker)
                           == mask(lights, *expected, live, atlas, seconds, flicker));
        };
    const auto view = glm::ortho(-4.f, 4.f, -3.f, 3.f);
    assert(!cache.matchesView(view, 0));
    check(view, 0, std::nullopt);
    // Flicker and carried-light movement do not require a full visibility scan.
    for (int i = 0; i < 100; ++i) {
        assert(cache.matchesView(view, 0));
        const std::optional<Coordinate> live{Coordinate{i % 7 - 3, i % 5 - 2, 0}};
        assert(cache.boundsWithLive(live) == reference(lights, view, 0, live));
    }
    check(view, 0, Coordinate{3, 2, 0});
    check(view, 0, Coordinate{30, 30, 0});
    check(view, 0, Coordinate{0, 0, 1});
    assert(!cache.matchesView(view, 1));
    check(view, 1, Coordinate{1, 1, 1});
    for (int i = 0; i < 6; ++i) {
        const float offset = static_cast<float>(i);
        const auto moved = glm::ortho(-5.f + offset, 5.f + offset, -4.f, 4.f);
        check(moved, 0, Coordinate{i, 1, 0});
    }
    const auto perspective = glm::perspective(glm::radians(60.f), 1.3f, .1f, 100.f)
                             * glm::lookAt(glm::vec3{5.f, -8.f, 20.f},
                                           glm::vec3{0.f},
                                           glm::vec3{0.f, 1.f, 0.f});
    check(perspective, 0, Coordinate{0, 0, 0});
    auto tinyPan = perspective;
    tinyPan[3][0] = std::nextafter(tinyPan[3][0], 100.f);
    assert(!cache.matchesView(tinyPan, 0));
    check(tinyPan, 0, std::nullopt);
    // A seed change with the same bounds must refresh mask membership/data.
    lights.back().seed = 801;
    cache.setLights(lights);
    assert(!cache.matchesView(tinyPan, 0));
    check(tinyPan, 0, std::nullopt);
    // Moving/deleting a permanent light, empty maps, and live-only illumination.
    lights.front().position = Coordinate{1, 1, 0};
    lights.erase(lights.begin() + 1);
    cache.setLights(lights);
    check(view, 0, Coordinate{0, 0, 0});
    lights.clear();
    cache.setLights(lights);
    check(view, 0, std::nullopt);
    check(view, 0, Coordinate{0, 0, 0});
    check(view, 0, Coordinate{30, 30, 0});
    auto behind = glm::mat4{1.f};
    behind[3][3] = -1.f;
    check(behind, 0, Coordinate{0, 0, 0});
    // A source whose room is outside the viewport can still contribute a halo.
    cache.setLights({LocatedLight{Coordinate{1, 0, 0}, 42}});
    const auto rimView = glm::ortho(0.f, .8f, 0.f, 1.f);
    cache.updateView(rimView, 0);
    const auto rim = cache.boundsWithLive(std::nullopt);
    assert(rim && rim->contains(Coordinate{1, 0, 0}) && rim->contains(Coordinate{0, 0, 0}));
    cache.setLights({LocatedLight{Coordinate{0, 0, 0}, 42, true}});
    cache.updateView(view, 0);
    assert(!cache.boundsWithLive(std::nullopt));
    assert(cache.setStreetLightsOn(true));
    assert(!cache.matchesView(view, 0));
    cache.updateView(view, 0);
    const auto streetBounds = cache.boundsWithLive(std::nullopt);
    assert(streetBounds && cache.maskLights(*streetBounds).size() == 1);
    assert(!cache.setStreetLightsOn(true));
    assert(cache.setStreetLightsOn(false));
    cache.updateView(view, 0);
    assert(!cache.boundsWithLive(std::nullopt));
    assert(cache.boundsWithLive(Coordinate{0, 0, 0})); // Carried light still works in daytime.
    std::cout << "Terrain halo visibility, street-lamp clock and pixel-equivalence checks passed\n";
}
