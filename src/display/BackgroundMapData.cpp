// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 The MMapper Authors

#include "BackgroundMapData.h"

#include "../global/progresscounter.h"
#include "../map/RoomHandle.h"
#include "../media/StackBlur.h"
#include "RoadIndex.h"

#include <algorithm>
#include <array>
#include <stdexcept>
#include <unordered_set>

#include <QImageReader>

namespace backgroundmap {
namespace {
int chunkIndex(const int value)
{
    // Floor division matters west/south of the coordinate origin.
    return value / CHUNK_SIZE - (value % CHUNK_SIZE < 0 ? 1 : 0);
}

int roadTexture(const ExternalRawRoom &room)
{
    RoadIndexMaskEnum mask = RoadIndexMaskEnum::NONE;
    for (const auto dir : ALL_EXITS_NESW) {
        if (room.getExit(dir).exitIsRoad()) {
            mask |= getRoadIndex(dir);
        }
    }
    return ROAD_TEXTURE_OFFSET + static_cast<int>(mask);
}
} // namespace

Geometry buildGeometry(const std::vector<ExternalRawRoom> &rooms, ProgressCounter &progress)
{
    Geometry result;
    std::unordered_set<Coordinate> seen;
    progress.setNewTask(ProgressMsg{"Build background terrain"}, rooms.size());
    for (const auto &room : rooms) {
        progress.step();
        const auto pos = room.getPosition();
        if (!seen.insert(pos).second) {
            throw std::runtime_error("Background map has duplicate room coordinates");
        }
        const auto terrain = room.getTerrainType();
        const int texture = terrain == RoomTerrainEnum::ROAD ? roadTexture(room)
                                                             : static_cast<int>(terrain);
        if (texture < 0 || texture >= ROAD_TEXTURE_OFFSET + NUM_ROAD_INDICES) {
            throw std::runtime_error("Invalid background terrain type");
        }
        auto &chunk = result[{pos.z, chunkIndex(pos.x), chunkIndex(pos.y)}];
        chunk.emplace_back(glm::ivec3{pos.x, pos.y, pos.z}, texture);
    }
    return result;
}

std::vector<QImage> loadTextures(const QStringList &files,
                                 const bool soften,
                                 ProgressCounter &progress)
{
    std::vector<QImage> result;
    progress.setNewTask(ProgressMsg{"Load background tiles"}, static_cast<size_t>(files.size()));
    for (const auto &file : files) {
        progress.step();
        QImageReader reader(file);
        const auto dimensions = reader.size();
        if (dimensions.width() > 4096 || dimensions.height() > 4096) {
            throw std::runtime_error("Background tile exceeds 4096 pixels");
        }
        reader.setScaledSize(QSize(TILE_SIZE, TILE_SIZE));
        auto image = reader.read();
        if (image.isNull()) {
            throw std::runtime_error(
                QStringLiteral("Cannot read background tile: %1").arg(file).toStdString());
        }
        image = image.scaled(TILE_SIZE, TILE_SIZE, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
        if (soften) {
            image = image.convertToFormat(QImage::Format_ARGB32_Premultiplied);
            mm::stackBlur(image, 2);
        }
        result.push_back(image.mirrored().convertToFormat(QImage::Format_RGBA8888));
    }
    return result;
}

ShadowGeometry buildShadow(const Map &map,
                           ProgressCounter &progress,
                           const RoomVisibility &visibility)
{
    ShadowGeometry result;
    progress.setNewTask(ProgressMsg{"Build foreground boundary shadow"}, map.getRoomsCount());
    if (visibility.hidesAllRooms()) {
        return result;
    }
    // Only exposed edges cast the diffuse skirt. Adjacent room interiors do
    // not accumulate dark seams. The foreground itself covers the inner edge.
    const std::array<Coordinate, 4> neighbors{Coordinate{0, -1, 0},
                                              Coordinate{1, 0, 0},
                                              Coordinate{0, 1, 0},
                                              Coordinate{-1, 0, 0}};
    const std::array<glm::vec2, 4> corners{glm::vec2{0, 0},
                                           glm::vec2{1, 0},
                                           glm::vec2{1, 1},
                                           glm::vec2{0, 1}};
    constexpr float spread = .24f;
    const glm::vec2 offset{.09f, -.09f};
    const Color inner{0.f, 0.f, 0.f, .28f};
    const Color outer{0.f, 0.f, 0.f, 0.f};
    for (const auto id : map.getRooms()) {
        progress.step();
        const auto room = map.getRoomHandle(id);
        if (!visibility.allows(room)) {
            continue;
        }
        const auto pos = room.getPosition();
        const glm::vec2 origin{static_cast<float>(pos.x), static_cast<float>(pos.y)};
        auto &vertices = result[pos.z];
        for (size_t edge = 0; edge < neighbors.size(); ++edge) {
            const auto &delta = neighbors[edge];
            if (const auto neighbor = map.findRoomHandle(
                    Coordinate{pos.x + delta.x, pos.y + delta.y, pos.z});
                neighbor && visibility.allows(neighbor)) {
                continue;
            }
            const glm::vec2 normal{static_cast<float>(delta.x), static_cast<float>(delta.y)};
            const auto start = origin + corners[edge] + offset;
            const auto end = origin + corners[(edge + 1) % 4] + offset;
            const auto a = start - normal * .12f;
            const auto b = end - normal * .12f;
            const auto c = end + normal * spread;
            const auto d = start + normal * spread;
            const auto z = static_cast<float>(pos.z);
            vertices.emplace_back(inner, glm::vec3{a, z});
            vertices.emplace_back(inner, glm::vec3{b, z});
            vertices.emplace_back(outer, glm::vec3{c, z});
            vertices.emplace_back(outer, glm::vec3{d, z});
        }
    }
    return result;
}

bool isChunkVisible(const ChunkKey key, const glm::mat4 &viewProjection)
{
    const auto [z, x, y] = key;
    std::array<glm::vec4, 4> clips;
    size_t index = 0;
    for (const auto dx : {0, 1}) {
        for (const auto dy : {0, 1}) {
            clips[index++] = viewProjection
                             * glm::vec4{static_cast<float>(x) * CHUNK_SIZE
                                             + static_cast<float>(dx * CHUNK_SIZE),
                                         static_cast<float>(y) * CHUNK_SIZE
                                             + static_cast<float>(dy * CHUNK_SIZE),
                                         static_cast<float>(z),
                                         1.f};
        }
    }
    for (int axis = 0; axis < 3; ++axis) {
        for (const auto sign : {-1.f, 1.f}) {
            if (std::all_of(clips.begin(), clips.end(), [=](const glm::vec4 &clip) {
                    return sign * clip[axis] > clip.w;
                })) {
                return false;
            }
        }
    }
    return true;
}
} // namespace backgroundmap
