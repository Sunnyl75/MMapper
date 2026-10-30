// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 The MMapper Authors

#include "../mapstorage/MapLoadHelper.h"
#include "BackgroundMapData.h"
#include "Filenames.h"
#include "RoadIndex.h"

#include <stdexcept>

#include <QDir>
#include <QFileInfo>

namespace backgroundmap {

Geometry loadGeometry(const QString &filename, const std::shared_ptr<ProgressCounter> &progress)
{
    auto source = MapSource::alloc(filename);
    auto storage = maploadhelper::detectAndCreateStorage(source, nullptr);
    storage->setProgressCounter(progress);
    auto data = storage->loadData();
    if (!data || data->rooms.empty()) {
        throw std::runtime_error("Background map contains no rooms");
    }
    return buildGeometry(data->rooms, *progress);
}

QStringList textureFiles(const QString &tileset, const QString &directory)
{
    QStringList result;
    const auto add = [&](const QString &current) {
        const auto name = QFileInfo(current).fileName();
        const auto builtin = QStringLiteral(":/pixmaps/") + name;
        if (tileset == QStringLiteral("foreground")) {
            result.push_back(current);
        } else if (tileset == QStringLiteral("custom")) {
            const QDir root(directory);
            const auto nested = root.filePath(QStringLiteral("pixmaps/") + name);
            const auto direct = root.filePath(name);
            result.push_back(QFileInfo(nested).isFile()   ? nested
                             : QFileInfo(direct).isFile() ? direct
                                                          : builtin);
        } else {
            result.push_back(builtin);
        }
    };
    for (size_t i = 0; i < NUM_ROOM_TERRAIN_TYPES; ++i) {
        add(getPixmapFilename(static_cast<RoomTerrainEnum>(i)));
    }
    for (int i = 0; i < NUM_ROAD_INDICES; ++i) {
        add(getPixmapFilename(TaggedRoad{static_cast<RoadIndexMaskEnum>(i)}));
    }
    return result;
}

} // namespace backgroundmap
