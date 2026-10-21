#pragma once
// SPDX-License-Identifier: GPL-2.0-or-later
#include "../map/Map.h"
#include "../opengl/OpenGLTypes.h"
#include "TerrainLightMask.h"
#include "TerrainLightVisibility.h"
#include "TerrainLightingDiagnostics.h"
#include "TerrainRoomLightRules.h"

#include <chrono>
#include <map>
#include <optional>

class OpenGL;
class MapData;
class GmcpMessage;

class TerrainRoomLighting final
{
    using Entry = terrain_room_light::LocatedLight;
    terrain_room_light::VisibilityCache m_visibility;
    TerrainLightingDiagnostics m_diagnostics;
    bool m_indexInitialized = false;
    std::map<RoomId, Entry> m_lights;
    // Learnt once per map load, then refreshed only for the entered room.
    std::map<Coordinate, uint8_t> m_walls;
    std::map<Coordinate, terrain_room_light::MaskAtlas::Clip> m_clips;
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

    void initializeIndex(const Map &map);
    bool updateRoom(const RoomHandle &room);
    void refreshVisibility();
    void updateWalls(const RoomHandle &room);
    const terrain_room_light::MaskAtlas::Clip &clipFor(Coordinate position);
    void upload(OpenGL &gl, const Bounds &bounds, double seconds);

public:
    TerrainLightingDiagnostics &diagnostics() { return m_diagnostics; }
    bool observe(const GmcpMessage &message);
    void clearObservation();
    void resetIndex();
    void updateCurrentRoom(const Map &map, RoomId id);
    void cleanup(OpenGL &gl);
    bool isAnimating() const { return m_animating; }
    GLRenderState prepare(OpenGL &gl,
                          const MapData &data,
                          int layer,
                          const glm::mat4 &viewProjection,
                          bool detailed,
                          bool streetLightsOn);
};
