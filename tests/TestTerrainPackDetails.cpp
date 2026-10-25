// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 The MMapper Authors
#include "display/TerrainSpritePacks.h"

#include <iostream>
#include <stdexcept>

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTemporaryDir>

namespace {
void require(const bool condition)
{
    if (!condition)
        throw std::runtime_error("Optional terrain pack detail regression");
}
void save(const QString &path, const QColor color)
{
    QImage image{96, 64, QImage::Format_ARGB32};
    image.fill(color);
    require(image.save(path));
}
} // namespace

int main(int argc, char **argv)
{
    QTemporaryDir temporary;
    require(temporary.isValid());
    const QDir root{temporary.path()};
    require(root.mkpath("terrain/shire"));
    const auto directory = root.filePath("terrain/shire");
    save(directory + "/terrain-road-cart-tracks.png", Qt::red);
    save(directory + "/terrain-city-wall-fringe.png", Qt::green);
    QImage bundled{96, 64, QImage::Format_ARGB32};
    bundled.fill(Qt::white);
    const auto global = [&](const QString &name) {
        if (name == "terrain-city-ground-v2.png" || name == "terrain-indoor-ground-v2.png") {
            QImage paving{64, 64, QImage::Format_ARGB32};
            paving.fill(Qt::white);
            return paving;
        }
        return bundled;
    };
    const auto load = [&]() {
        return terrain_sprites::loadSpritePacks(root.filePath("unused.json"),
                                                bundled,
                                                {root.filePath("terrain")});
    };
    auto sprites = load();
    const auto packs = terrain_sprites::loadMaterialPacks(sprites, global);
    const auto &shire = packs.find("the Shire");
    require(packs.index("the Shire") != 0 && packs.index("SHIRE") == packs.index("the Shire"));
    require(shire.cartTracks >= 0 && shire.cityWallFringe >= 0);
    require(packs.find("").cartTracks < 0 && packs.find("unknown").cityWallFringe < 0);
    const auto lookup = terrain_sprites::packLookup(sprites, packs, {});
    const auto track = lookup.front().pixelColor(200, packs.index("the Shire"));
    require(track.alpha() == 255 && track.red() + track.green() * 256 == shire.cartTracks * 16);
    require(lookup.front().pixelColor(201, packs.index("the Shire")).alpha() == 255);
    require(lookup.front().pixelColor(200, 0).alpha() == 0);

    // Malformed optional art must disappear rather than borrowing another pack.
    require(QFile::remove(directory + "/terrain-road-cart-tracks.png"));
    QFile broken{directory + "/terrain-city-wall-fringe.png"};
    require(broken.open(QIODevice::WriteOnly) && broken.write("not PNG") == 7);
    broken.close();
    auto missingSprites = load();
    const auto missing = terrain_sprites::loadMaterialPacks(missingSprites, global);
    require(missing.find("the Shire").cartTracks < 0
            && missing.find("the Shire").cityWallFringe < 0);

    // Optional input also falls through an invalid higher-priority root.
    require(root.mkpath("assets/shire"));
    save(root.filePath("assets/shire/terrain-city-wall-fringe.png"), Qt::blue);
    auto fallbackSprites = terrain_sprites::loadSpritePacks(root.filePath("unused.json"),
                                                            bundled,
                                                            {root.filePath("terrain"),
                                                             root.filePath("assets")});
    const auto fallback = terrain_sprites::loadMaterialPacks(fallbackSprites, global);
    require(fallback.find("the Shire").cityWallFringe >= 0);

    // Optional argument exercises the delivered real pack and every supplied family.
    if (argc > 2) {
        const QDir actual{QString::fromLocal8Bit(argv[1])};
        const QDir assets{QString::fromLocal8Bit(argv[2])};
        const QImage trees{assets.filePath("terrain-trees-seasons.png")};
        auto actualSprites = terrain_sprites::loadSpritePacks(root.filePath("unused.json"),
                                                              trees,
                                                              {QFileInfo{actual.path()}.path()});
        const auto actualPacks = terrain_sprites::loadMaterialPacks(actualSprites,
                                                                    [&](const QString &name) {
                                                                        return QImage{
                                                                            assets.filePath(name)};
                                                                    });
        const auto &selected = actualPacks.find("the Shire");
        const auto &defaultPack = actualPacks.find("unknown");
        require(actualPacks.index("the Shire") != 0);
        require(selected.cartTracks >= 0 && selected.cityWallFringe >= 0);
        using F = terrain_materials::Family;
        for (const auto family : {F::FOREST_GROUND,
                                  F::FIELD_GROUND,
                                  F::FIELD_DETAIL,
                                  F::ROAD,
                                  F::CITY_GROUND,
                                  F::INDOOR_GROUND,
                                  F::TIMBER_WALL,
                                  F::STONE_WALL,
                                  F::CITY_EAVE,
                                  F::WALL_FINISH,
                                  F::DOOR})
            require(selected.has(family) && selected.atlas(family) != defaultPack.atlas(family));
        const auto door = terrain_sprites::alignDoorSheet(
            QImage{actual.filePath("terrain-doors.png")});
        require(!door.isNull());
        int removed = 0;
        for (int x = 0; x < 256; ++x)
            for (int y = 0; y < 256; ++y)
                removed += door.pixelColor(x, y).alpha() > door.pixelColor(x, y + 256).alpha();
        require(removed > 100); // Open frame removes the central round leaf.
        const auto beforeCount = static_cast<int>(actualSprites.atlases.size());
        require(beforeCount <= 256);
        std::cout << "PASS real Shire pack selection, supplied families, details and open doors\n";
    }
    std::cout << "PASS optional terrain details, availability, fallback and area normalization\n";
}
