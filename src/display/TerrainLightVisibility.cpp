// SPDX-License-Identifier: GPL-2.0-or-later
#include "TerrainLightVisibility.h"

#include <algorithm>
#include <utility>

namespace terrain_room_light {
namespace {
void include(std::optional<Bounds> &bounds, const Coordinate position)
{
    if (!bounds) {
        bounds = Bounds{position, position};
        return;
    }
    bounds->min.x = std::min(bounds->min.x, position.x);
    bounds->min.y = std::min(bounds->min.y, position.y);
    bounds->max.x = std::max(bounds->max.x, position.x);
    bounds->max.y = std::max(bounds->max.y, position.y);
}
} // namespace

void VisibilityCache::setLights(std::vector<LocatedLight> lights)
{
    m_lights = std::move(lights);
    m_valid = false;
    m_visibleBounds.reset();
    m_maskBounds.reset();
    m_maskLights.clear();
}

bool VisibilityCache::setStreetLightsOn(const bool on)
{
    if (on == m_streetLightsOn)
        return false;
    m_streetLightsOn = on;
    m_valid = false;
    m_maskBounds.reset();
    return true;
}

bool VisibilityCache::matchesView(const glm::mat4 &viewProjection, const int layer) const
{
    if (!m_valid || layer != m_layer)
        return false;
    // Exact equality is intentional: even a small pan/zoom can expose a room.
    for (int column = 0; column < 4; ++column)
        for (int row = 0; row < 4; ++row)
            if (!utils::isSameFloat(viewProjection[column][row], m_viewProjection[column][row]))
                return false;
    return true;
}

bool VisibilityCache::visibleAt(const Coordinate position) const
{
    glm::vec2 lo{1.e20f}, hi{-1.e20f};
    bool inFront = false;
    for (int corner = 0; corner < 4; ++corner) {
        const auto clip = m_viewProjection
                          * glm::vec4(position.to_vec3()
                                          + glm::vec3{static_cast<float>(corner & 1) * 2.f - 0.5f,
                                                      static_cast<float>(corner >> 1) * 2.f - 0.5f,
                                                      0.f},
                                      1.f);
        if (clip.w <= 0.f)
            continue;
        inFront = true;
        const auto point = glm::vec2{clip} / clip.w;
        lo = glm::min(lo, point);
        hi = glm::max(hi, point);
    }
    return inFront && lo.x <= 1.f && hi.x >= -1.f && lo.y <= 1.f && hi.y >= -1.f;
}

void VisibilityCache::updateView(const glm::mat4 &viewProjection, const int layer)
{
    m_viewProjection = viewProjection;
    m_layer = layer;
    m_visibleBounds.reset();
    for (const auto &light : m_lights)
        if ((!light.nocturnal || m_streetLightsOn) && light.position.z == layer
            && visibleAt(light.position))
            include(m_visibleBounds, light.position);
    m_valid = true;
}

std::optional<Bounds> VisibilityCache::boundsWithLive(const std::optional<Coordinate> live) const
{
    auto bounds = m_visibleBounds;
    if (live && live->z == m_layer && visibleAt(*live))
        include(bounds, *live);
    if (bounds) {
        --bounds->min.x;
        --bounds->min.y;
        ++bounds->max.x;
        ++bounds->max.y;
    }
    return bounds;
}

const std::vector<LocatedLight> &VisibilityCache::maskLights(const Bounds &bounds)
{
    if (!m_maskBounds || *m_maskBounds != bounds) {
        m_maskLights.clear();
        for (const auto &light : m_lights)
            if ((!light.nocturnal || m_streetLightsOn) && bounds.contains(light.position))
                m_maskLights.push_back(light);
        m_maskBounds = bounds;
    }
    // Preserve the original room-id order and include every light within the
    // rectangle, including off-screen rooms inside it, as the old renderer did.
    return m_maskLights;
}
} // namespace terrain_room_light
