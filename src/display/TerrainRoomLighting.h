#pragma once
// SPDX-License-Identifier: GPL-2.0-or-later
#include "../map/Map.h"
#include "../opengl/OpenGLTypes.h"
#include "TerrainLightMask.h"
#include "TerrainRoomLightRules.h"

#include <chrono>
#include <map>
#include <optional>

class OpenGL;
class MapData;
class GmcpMessage;

class TerrainRoomLighting final
{
    struct Entry final
    {
        Coordinate position;
        uint32_t seed = 0;
        bool operator==(const Entry &) const = default;
    };
    std::optional<Map> m_map;
    std::map<RoomId, Entry> m_lights;
    SharedMMTexture m_texture;
    std::optional<Bounds> m_gridBounds;
    terrain_room_light::Observation m_observation;
    std::optional<terrain_room_light::MaskAtlas> m_atlas;
    std::optional<Coordinate> m_live;
    uint32_t m_liveSeed = 0;
    int m_strength = -1;
    int m_flicker = -1;
    int64_t m_frame = -1;
    bool m_dirty = true;
    bool m_animating = false;
    std::chrono::steady_clock::time_point m_started = std::chrono::steady_clock::now();

    void update(const Map &map);
    void upload(OpenGL &gl, const Bounds &bounds, double seconds);

public:
    bool observe(const GmcpMessage &message);
    void clearObservation();
    void cleanup(OpenGL &gl);
    bool isAnimating() const { return m_animating; }
    GLRenderState prepare(
        OpenGL &gl, const MapData &data, int layer, const glm::mat4 &viewProjection, bool detailed);
};
