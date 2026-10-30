#pragma once
// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 The MMapper Authors

#include "../map/Map.h"
#include "../opengl/OpenGLTypes.h"

#include <map>
#include <memory>
#include <tuple>
#include <vector>

#include <QImage>
#include <QStringList>

class ProgressCounter;

namespace backgroundmap {

// CPU-only data. No MapData, selection, game IDs, text, exits or editing state
// survive loading. The ordinary map readers remain the single format authority.
constexpr int CHUNK_SIZE = 64;
constexpr int TILE_SIZE = 64;
constexpr int ROAD_TEXTURE_OFFSET = static_cast<int>(NUM_ROOM_TERRAIN_TYPES);
static_assert(NUM_ROOM_TERRAIN_TYPES + 16 <= 256, "Background tiles must fit the room shader");
using ChunkKey = std::tuple<int, int, int>; // z, chunk x, chunk y
using Geometry = std::map<ChunkKey, std::vector<RoomQuadTexVert>>;
using ShadowGeometry = std::map<int, std::vector<ColorVert>>;

NODISCARD Geometry loadGeometry(const QString &filename,
                                const std::shared_ptr<ProgressCounter> &progress);
NODISCARD Geometry buildGeometry(const std::vector<ExternalRawRoom> &rooms,
                                 ProgressCounter &progress);
// Resolve paths on the GUI thread; workers never access global configuration.
NODISCARD QStringList textureFiles(const QString &tileset, const QString &directory);
NODISCARD std::vector<QImage> loadTextures(const QStringList &files,
                                           bool soften,
                                           ProgressCounter &progress);
NODISCARD ShadowGeometry buildShadow(const Map &map, ProgressCounter &progress);
NODISCARD bool isChunkVisible(ChunkKey key, const glm::mat4 &viewProjection);

} // namespace backgroundmap
