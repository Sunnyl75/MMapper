// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 The MMapper Authors
#include "display/TerrainSpritePacks.h"

#include <iostream>
#include <stdexcept>

#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>

static void require(const bool ok)
{
    if (!ok)
        throw std::runtime_error("Terrain pack fallback regression");
}
static void write(const QString &path, const QByteArray &data)
{
    QFile file{path};
    require(file.open(QIODevice::WriteOnly));
    require(file.write(data) == data.size());
}
static QColor tile(const terrain_sprites::SpritePacks &packs,
                   const int base,
                   const int season,
                   const int variant)
{
    const int index = season * 6 + variant;
    const auto &image = packs.atlases.at(static_cast<std::size_t>(base + index / 16)).front();
    return image.pixelColor((index % 4) * 128 + 64, ((index % 16) / 4) * 128 + 64);
}
int main()
{
    QTemporaryDir temp;
    require(temp.isValid());
    const QDir dir{temp.path()};
    require(dir.mkpath("default") && dir.mkpath("local") && dir.mkpath("evergreen"));
    const auto sheet = [](const QColor color) {
        QImage i{96, 64, QImage::Format_ARGB32};
        i.fill(color);
        return i;
    };
    QImage bundled{192, 128, QImage::Format_ARGB32};
    for (int y = 0; y < 128; ++y)
        for (int x = 0; x < 192; ++x)
            bundled.setPixelColor(x, y, QColor{(x / 32) * 30, (y / 32) * 50, 30});
    require(sheet(Qt::yellow).save(dir.filePath("default/autumn.png")));
    require(sheet(Qt::red).save(dir.filePath("local/spring.png")));
    require(sheet(Qt::green).save(dir.filePath("evergreen/all.png")));
    write(dir.filePath("packs.json"),
          R"({"superzones":{"The Shire":"local","Alias":"local","Pines":"evergreen","Missing":"missing","Bad":"../elsewhere"}})");
    auto packs = terrain_sprites::loadSpritePacks(dir.filePath("packs.json"), bundled);
    require(packs.atlases.size()
            == 6); // Default, local, evergreen. Missing and aliases share existing pages.
    require(packs.areas.size() == 4 && !packs.areas.contains("Bad"));
    require(packs.areas.at("Alias") == packs.areas.at("The Shire"));
    require(!packs.areas.contains("the shire"));
    const int local = packs.areas.at("The Shire");
    const int pine = packs.areas.at("Pines");
    const int missing = packs.areas.at("Missing");
    for (int v = 0; v < 6; ++v) {
        require(tile(packs, local, 1, v) == Qt::red);                // Exact superzone season.
        require(tile(packs, local, 3, v) == Qt::yellow);             // Missing -> global season.
        require(tile(packs, local, 2, v) == QColor(v * 30, 50, 30)); // Missing -> bundled summer.
        require(tile(packs, missing, 0, v)
                == QColor(v * 30, 150, 30)); // Entire missing pack -> bundled winter.
        for (int s = 0; s < 4; ++s)
            require(tile(packs, pine, s, v) == Qt::green); // Explicit all-season pack.
    }
    // Invalid overrides must fall back too, including invalid dimensions.
    write(dir.filePath("local/summer.png"), "not a PNG");
    auto broken = terrain_sprites::loadSpritePacks(dir.filePath("packs.json"), bundled);
    require(tile(broken, broken.areas.at("The Shire"), 2, 5) == QColor(150, 50, 30));
    require(sheet(Qt::blue).save(dir.filePath("default/all.png")));
    auto all = terrain_sprites::loadSpritePacks(dir.filePath("packs.json"), bundled);
    require(tile(all, all.areas.at("The Shire"), 0, 0) == Qt::blue);
    require(tile(all, all.areas.at("The Shire"), 3, 0) == Qt::yellow);
    write(dir.filePath("packs.json"), "broken json");
    require(terrain_sprites::loadSpritePacks(dir.filePath("packs.json"), bundled).areas.empty());
    write(dir.filePath("packs.json"), R"({"superzones":{}})");
    auto small = terrain_sprites::loadSpritePacks(dir.filePath("packs.json"), bundled);
    require(small.atlases.size() == 2);
    for (const auto &atlas : small.atlases) {
        require(atlas.size() == 8);
        for (std::size_t level = 0; level < 8; ++level)
            require(atlas[level].width() == (512 >> level));
    }
    QJsonObject zones;
    for (int i = 0; i < 45; ++i)
        zones.insert(QString::number(i), "local");
    write(dir.filePath("packs.json"), QJsonDocument(QJsonObject{{"superzones", zones}}).toJson());
    auto many = terrain_sprites::loadSpritePacks(dir.filePath("packs.json"), bundled);
    require(many.areas.size() == 44 && many.atlases.size() == 4);
    auto expanded = small;
    const int ground = terrain_sprites::appendSeasonalSheet(expanded, bundled);
    require(ground == 2 && expanded.atlases.size() == 4);
    for (int season = 0; season < 4; ++season)
        for (int variant = 0; variant < 6; ++variant)
            require(tile(expanded, ground, season, variant)
                    == QColor(variant * 30, ((season + 3) % 4) * 50, 30));
    bool rejected = false;
    try {
        (void) terrain_sprites::appendSeasonalSheet(expanded, QImage{20, 20, QImage::Format_ARGB32});
    } catch (const std::runtime_error &) {
        rejected = true;
    }
    require(rejected && expanded.atlases.size() == 4);
    std::cout << "Superzone aliases, all fallback levels, invalid assets and atlas layout passed\n";
}
