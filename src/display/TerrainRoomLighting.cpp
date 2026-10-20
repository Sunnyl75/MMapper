// SPDX-License-Identifier: GPL-2.0-or-later
#include "TerrainRoomLighting.h"

#include "../configuration/configuration.h"
#include "../mapdata/mapdata.h"
#include "../opengl/OpenGL.h"
#include "../proxy/GmcpMessage.h"
#include "Textures.h"

#include <algorithm>
#include <cmath>

#include <glm/gtc/matrix_inverse.hpp>

#include <QJsonDocument>
#include <QOpenGLTexture>
#include <QPainter>

void TerrainRoomLighting::clearObservation()
{
    m_observation.clear();
}
bool TerrainRoomLighting::observe(const GmcpMessage &message)
{
    const auto before = m_observation;
    if (message.isEventMoved() || message.isCoreGoodbye()) {
        clearObservation();
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

void TerrainRoomLighting::update(const Map &map)
{
    if (m_map && m_map->isSamePointer(map))
        return;
    // Cheap field comparisons on a map revision; only changed lighting inputs are rescanned.
    for (auto it = m_lights.begin(); it != m_lights.end();) {
        if (!map.findRoomHandle(it->first)) {
            it = m_lights.erase(it);
            m_dirty = true;
        } else
            ++it;
    }
    map.getRooms().for_each([this, &map](const RoomId id) {
        const auto room = map.getRoomHandle(id);
        const auto old = m_map ? m_map->findRoomHandle(id) : RoomHandle{};
        if (old && old.getDescription() == room.getDescription()
            && old.getLightType() == room.getLightType()
            && old.getTerrainType() == room.getTerrainType()
            && old.getPosition() == room.getPosition()
            && m_map->getExternalRoomId(id) == map.getExternalRoomId(id))
            return;
        const bool lit = terrain_room_light::classify(room.getTerrainType(),
                                                      room.getLightType(),
                                                      room.getDescription().getStdStringViewUtf8())
                         != terrain_room_light::Evidence::NONE;
        const auto existing = m_lights.find(id);
        if (lit) {
            const Entry next{room.getPosition(), map.getExternalRoomId(id).value()};
            if (existing == m_lights.end() || !(existing->second == next)) {
                m_lights[id] = next;
                m_dirty = true;
            }
        } else if (existing != m_lights.end()) {
            m_lights.erase(existing);
            m_dirty = true;
        }
    });
    m_map = map;
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
    const int width = (bounds.max.x - bounds.min.x + 1) * pixels;
    const int height = (bounds.max.y - bounds.min.y + 1) * pixels;
    if (!m_atlas)
        m_atlas.emplace(QImage(QStringLiteral(":/pixmaps/terrain-light-masks.png")));
    QImage image(width, height, QImage::Format_RGBA8888);
    image.fill(0);
    QPainter painter(&image);
    painter.setCompositionMode(QPainter::CompositionMode_Source);
    const auto draw = [&](const Coordinate position, const uint32_t seed) {
        if (!bounds.contains(position))
            return;
        painter.drawImage((position.x - bounds.min.x) * pixels,
                          (position.y - bounds.min.y) * pixels,
                          m_atlas->tile(seed, seconds, m_strength, m_flicker));
    };
    for (const auto &[id, entry] : m_lights) {
        std::ignore = id;
        draw(entry.position, entry.seed);
    }
    if (m_live) {
        // Overwrite, rather than add to, an already illuminated room.
        draw(*m_live, m_liveSeed);
    }
    painter.end();
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
                                           const bool detailed)
{
    GLRenderState state;
    m_animating = false;
    const auto &settings = getConfig().canvas;
    if (!detailed || !settings.organicTerrain.get() || !settings.roomIllumination.get()
        || settings.roomLightStrength.get() <= 0 || data.isEmpty())
        return state;
    const auto &map = data.getCurrentMap();
    update(map);
    const auto visibleAt = [&viewProjection](const Coordinate position) {
        glm::vec2 lo{1.e20f}, hi{-1.e20f};
        bool inFront = false;
        for (int corner = 0; corner < 4; ++corner) {
            const auto clip = viewProjection
                              * glm::vec4(position.to_vec3()
                                              + glm::vec3{static_cast<float>(corner & 1),
                                                          static_cast<float>(corner >> 1),
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
    };
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
    std::optional<Bounds> visibleBounds;
    const auto include = [&](const Coordinate position) {
        if (position.z != layer || !visibleAt(position))
            return;
        if (!visibleBounds)
            visibleBounds = Bounds{position, position};
        else {
            visibleBounds->min.x = std::min(visibleBounds->min.x, position.x);
            visibleBounds->min.y = std::min(visibleBounds->min.y, position.y);
            visibleBounds->max.x = std::max(visibleBounds->max.x, position.x);
            visibleBounds->max.y = std::max(visibleBounds->max.y, position.y);
        }
    };
    for (const auto &[id, entry] : m_lights) {
        std::ignore = id;
        include(entry.position);
    }
    if (m_live)
        include(*m_live);
    if (!visibleBounds)
        return state;
    // Cache only the visible region, never an entire sparse map.
    const Bounds bounds = *visibleBounds;
    if (bounds.max.x - bounds.min.x >= 256 || bounds.max.y - bounds.min.y >= 256)
        return state; // Beyond useful sprite-detail range; allocation stays <= 64 MiB.
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
    m_animating = settings.roomLightFlicker.get() > 0;
    return state;
}
