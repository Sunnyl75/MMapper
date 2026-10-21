// SPDX-License-Identifier: GPL-2.0-or-later
#include "TerrainRoomLighting.h"

#include "../configuration/configuration.h"
#include "../mapdata/mapdata.h"
#include "../opengl/OpenGL.h"
#include "../proxy/GmcpMessage.h"
#include "Textures.h"

#include <algorithm>
#include <cmath>
#include <utility>
#include <vector>

#include <glm/gtc/matrix_inverse.hpp>

#include <QJsonDocument>
#include <QOpenGLTexture>

void TerrainRoomLighting::clearObservation()
{
    m_observation.clear();
}
bool TerrainRoomLighting::observe(const GmcpMessage &message)
{
    auto timing = m_diagnostics.measure(TerrainLightingDiagnostics::Stage::OBSERVATION);
    if (message.isEventMoved())
        m_diagnostics.movement();
    const auto before = m_observation;
    if (message.isCoreGoodbye()) {
        clearObservation();
    } else if (message.isEventMoved()) {
        m_observation.moved();
    } else {
        if ((!message.isRoomInfo() && !message.isCharVitals()) || !message.getJson())
            return false;
        const auto doc = QJsonDocument::fromJson(message.getJson()->toQString().toUtf8());
        if (!doc.isObject())
            return false;
        if (message.isRoomInfo())
            m_observation.room(doc.object());
        else
            m_observation.vitals(doc.object());
    }
    return before != m_observation;
}

void TerrainRoomLighting::resetIndex()
{
    m_indexInitialized = false;
    m_lights.clear();
    m_walls.clear();
    m_clips.clear();
    m_visibility.setLights({});
    m_live.reset();
    m_liveSeed = 0;
    m_dirty = true;
    m_diagnostics.cachedLights(0);
}

bool TerrainRoomLighting::updateRoom(const RoomHandle &room)
{
    updateWalls(room);
    const auto id = room.getId();
    const auto evidence = terrain_room_light::classify(room.getTerrainType(),
                                                       room.getLightType(),
                                                       room.getDescription().getStdStringViewUtf8(),
                                                       room.getContents().getStdStringViewUtf8());
    const bool lit = evidence != terrain_room_light::Evidence::NONE;
    const auto existing = m_lights.find(id);
    if (!lit) {
        if (existing == m_lights.end())
            return false;
        m_lights.erase(existing);
        return true;
    }
    const Entry next{room.getPosition(),
                     room.getIdExternal().value(),
                     evidence == terrain_room_light::Evidence::STREET_LAMP};
    if (existing != m_lights.end() && existing->second == next)
        return false;
    m_lights[id] = next;
    return true;
}

void TerrainRoomLighting::updateWalls(const RoomHandle &room)
{
    const auto pos = room.getPosition();
    uint8_t walls = 0;
    for (const auto dir : ALL_EXITS_NESW) {
        const auto flags = room.getExit(dir).getExitFlags();
        if (!flags.isExit() || flags.isDoor())
            walls |= static_cast<uint8_t>(1u << static_cast<unsigned>(dir));
    }
    const auto found = m_walls.find(pos);
    if (found != m_walls.end() && found->second == walls)
        return;
    m_walls[pos] = walls;
    // Only nine source clips can depend on this room. No map-wide work here.
    for (int y = -1; y <= 1; ++y)
        for (int x = -1; x <= 1; ++x)
            m_clips.erase(pos + Coordinate{x, y, 0});
    m_dirty = true;
}

const terrain_room_light::MaskAtlas::Clip &TerrainRoomLighting::clipFor(const Coordinate position)
{
    const auto existing = m_clips.find(position);
    if (existing != m_clips.end())
        return existing->second;
    terrain_room_light::WallNeighborhood neighbours;
    for (int y = -1; y <= 1; ++y)
        for (int x = -1; x <= 1; ++x) {
            const auto found = m_walls.find(position + Coordinate{x, y, 0});
            if (found != m_walls.end())
                neighbours.walls[static_cast<size_t>((y + 1) * 3 + x + 1)] = found->second;
        }
    return m_clips.emplace(position, terrain_room_light::MaskAtlas::clip(neighbours)).first->second;
}

void TerrainRoomLighting::refreshVisibility()
{
    std::vector<Entry> lights;
    lights.reserve(m_lights.size());
    for (const auto &[id, entry] : m_lights) {
        std::ignore = id;
        lights.push_back(entry);
    }
    m_visibility.setLights(std::move(lights));
    m_dirty = true;
    m_diagnostics.cachedLights(m_lights.size());
}

void TerrainRoomLighting::initializeIndex(const Map &map)
{
    if (m_indexInitialized)
        return;
    auto timing = m_diagnostics.measure(TerrainLightingDiagnostics::Stage::INDEX);
    // One full scan per map load, not per map revision or movement.
    map.getRooms().for_each(
        [this, &map](const RoomId id) { std::ignore = updateRoom(map.getRoomHandle(id)); });
    m_indexInitialized = true;
    refreshVisibility();
}

void TerrainRoomLighting::updateCurrentRoom(const Map &map, const RoomId id)
{
    // Before first use, the initial scan will include the latest room data.
    // Do not initialize the whole index on the movement callback.
    if (!m_indexInitialized)
        return;
    auto timing = m_diagnostics.measure(TerrainLightingDiagnostics::Stage::CURRENT_ROOM_INDEX);
    const auto room = map.findRoomHandle(id);
    const bool changed = room ? updateRoom(room) : m_lights.erase(id) > 0;
    if (changed)
        refreshVisibility();
}

void TerrainRoomLighting::cleanup(OpenGL &gl)
{
    if (m_texture) {
        gl.setTextureLookup(m_texture->getId(), {});
        m_texture.reset();
    }
    m_gridBounds.reset();
    m_dirty = true;
    m_animating = false;
}

void TerrainRoomLighting::upload(OpenGL &gl, const Bounds &bounds, const double seconds)
{
    constexpr int pixels = terrain_room_light::MaskAtlas::PIXELS;
    auto maskTiming
        = std::optional<TerrainLightingDiagnostics::Scope>{std::in_place,
                                                           m_diagnostics,
                                                           TerrainLightingDiagnostics::Stage::MASK};
    const int width = (bounds.max.x - bounds.min.x + 1) * pixels;
    const int height = (bounds.max.y - bounds.min.y + 1) * pixels;
    if (!m_atlas)
        m_atlas.emplace(QImage(QStringLiteral(":/pixmaps/terrain-light-masks.png")));
    QImage image(width, height, QImage::Format_RGBA8888);
    image.fill(0);
    const auto draw = [&](const Coordinate position, const uint32_t seed) {
        if (!bounds.contains(position))
            return;
        auto tile = m_atlas->tile(seed, seconds, m_strength, m_flicker);
        terrain_room_light::MaskAtlas::applyClip(tile, clipFor(position));
        terrain_room_light::MaskAtlas::composite(image,
                                                 tile,
                                                 (position.x - bounds.min.x) * pixels
                                                     - terrain_room_light::MaskAtlas::HALO_OFFSET,
                                                 (position.y - bounds.min.y) * pixels
                                                     - terrain_room_light::MaskAtlas::HALO_OFFSET);
    };
    for (const auto &entry : m_visibility.maskLights(bounds))
        if (!m_live || entry.position != *m_live)
            draw(entry.position, entry.seed);
    if (m_live)
        draw(*m_live, m_liveSeed);
    maskTiming.reset();
    auto uploadTiming = m_diagnostics.measure(TerrainLightingDiagnostics::Stage::UPLOAD);
    m_diagnostics.uploaded(width, height);
    if (m_texture && m_gridBounds
        && (m_gridBounds->max.x - m_gridBounds->min.x + 1) * pixels == width
        && (m_gridBounds->max.y - m_gridBounds->min.y + 1) * pixels == height) {
        m_texture->get()->setData(QOpenGLTexture::RGBA, QOpenGLTexture::UInt8, image.constBits());
    } else {
        cleanup(gl);
        m_texture = MMTexture::alloc(
            QOpenGLTexture::Target2D,
            [&image, width, height](QOpenGLTexture &texture) {
                texture.setFormat(QOpenGLTexture::RGBA8_UNorm);
                texture.setSize(width, height);
                texture.setMipLevels(1);
                texture.allocateStorage(QOpenGLTexture::RGBA, QOpenGLTexture::UInt8);
                texture.setData(QOpenGLTexture::RGBA, QOpenGLTexture::UInt8, image.constBits());
                texture.setMinMagFilters(QOpenGLTexture::Linear, QOpenGLTexture::Linear);
                texture.setWrapMode(QOpenGLTexture::ClampToEdge);
            },
            true);
        m_texture->setId(allocateTextureId());
        gl.setTextureLookup(m_texture->getId(), m_texture);
    }
    m_gridBounds = bounds;
    m_dirty = false;
}

GLRenderState TerrainRoomLighting::prepare(OpenGL &gl,
                                           const MapData &data,
                                           const int layer,
                                           const glm::mat4 &viewProjection,
                                           const bool detailed,
                                           const bool streetLightsOn)
{
    auto timing = m_diagnostics.measure(TerrainLightingDiagnostics::Stage::PREPARE);
    GLRenderState state;
    m_animating = false;
    const auto &settings = getConfig().canvas;
    if (!detailed || !settings.organicTerrain.get() || !settings.roomIllumination.get()
        || settings.roomLightStrength.get() <= 0 || data.isEmpty())
        return state;
    const auto &map = data.getCurrentMap();
    initializeIndex(map);
    if (m_visibility.setStreetLightsOn(streetLightsOn))
        m_dirty = true;
    auto visibilityTiming = std::optional<TerrainLightingDiagnostics::Scope>{
        std::in_place, m_diagnostics, TerrainLightingDiagnostics::Stage::VISIBILITY};
    if (!m_visibility.matchesView(viewProjection, layer)) {
        auto rebuildTiming = m_diagnostics.measure(
            TerrainLightingDiagnostics::Stage::VISIBILITY_REBUILD);
        m_visibility.updateView(viewProjection, layer);
    }
    const auto current = data.getCurrentRoom();
    const bool live = current && current.getPosition().z == layer
                      && m_observation.appliesTo(current.getServerId().asUint32());
    std::optional<Coordinate> nextLive;
    uint32_t nextLiveSeed = 0;
    if (live) {
        nextLive = current.getPosition();
        nextLiveSeed = map.getExternalRoomId(current.getId()).value();
    }
    if (nextLive != m_live || nextLiveSeed != m_liveSeed) {
        m_live = nextLive;
        m_liveSeed = nextLiveSeed;
        m_dirty = true;
    }
    const auto visibleBounds = m_visibility.boundsWithLive(m_live);
    if (!visibleBounds)
        return state;
    // Cache only the visible region, never an entire sparse map.
    const Bounds bounds = *visibleBounds;
    if (bounds.max.x - bounds.min.x >= 256 || bounds.max.y - bounds.min.y >= 256)
        return state; // Beyond useful sprite-detail range; allocation stays <= 64 MiB.
    visibilityTiming.reset();
    const auto elapsed = std::chrono::steady_clock::now() - m_started;
    const double seconds = std::chrono::duration<double>(elapsed).count();
    const int64_t frame = settings.roomLightFlicker.get() > 0
                              ? static_cast<int64_t>(seconds / 0.084)
                              : 0;
    if (m_strength != settings.roomLightStrength.get()
        || m_flicker != settings.roomLightFlicker.get()) {
        m_strength = settings.roomLightStrength.get();
        m_flicker = settings.roomLightFlicker.get();
        m_dirty = true;
    }
    if (m_dirty || !m_gridBounds || *m_gridBounds != bounds || frame != m_frame) {
        upload(gl, bounds, static_cast<double>(frame) * 0.084);
        m_frame = frame;
    }
    auto &uniforms = state.uniforms;
    uniforms.textures[1] = m_texture->getId();
    uniforms.roomLightGrid = glm::ivec4{bounds.min.x,
                                        bounds.min.y,
                                        bounds.max.x - bounds.min.x + 1,
                                        bounds.max.y - bounds.min.y + 1};
    uniforms.roomLightParams = glm::vec4{1.f, 0.f, 0.f, static_cast<float>(layer)};
    uniforms.roomLightInverse = glm::inverse(viewProjection);
    m_diagnostics.active();
    m_animating = settings.roomLightFlicker.get() > 0;
    return state;
}
