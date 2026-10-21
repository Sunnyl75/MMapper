#pragma once
// SPDX-License-Identifier: GPL-2.0-or-later
#include "../map/coordinate.h"

#include <cstdint>
#include <optional>
#include <vector>

namespace terrain_room_light {
struct LocatedLight final
{
    Coordinate position;
    uint32_t seed = 0;
    bool nocturnal = false;
    bool operator==(const LocatedLight &) const = default;
};

// CPU-only cache. Its keys do not include animation time, brightness or the
// player position: moving a carried light tests just that one extra room.
class VisibilityCache final
{
    std::vector<LocatedLight> m_lights;
    std::vector<LocatedLight> m_maskLights;
    std::optional<Bounds> m_visibleBounds;
    std::optional<Bounds> m_maskBounds;
    glm::mat4 m_viewProjection{1.f};
    int m_layer = 0;
    bool m_valid = false;
    bool m_streetLightsOn = false;

    bool visibleAt(Coordinate position) const;

public:
    void setLights(std::vector<LocatedLight> lights);
    bool setStreetLightsOn(bool on);
    bool matchesView(const glm::mat4 &viewProjection, int layer) const;
    void updateView(const glm::mat4 &viewProjection, int layer);
    std::optional<Bounds> boundsWithLive(std::optional<Coordinate> live) const;
    const std::vector<LocatedLight> &maskLights(const Bounds &bounds);
};
} // namespace terrain_room_light
