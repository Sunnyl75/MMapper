// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 The MMapper Authors
#include "display/TerrainSpritePacks.h"

#include <stdexcept>

#include <QDir>
#include <QFile>
#include <QTemporaryDir>

namespace {
void require(const bool condition)
{
    if (!condition)
        throw std::runtime_error("Named terrain pack regression");
}
void save(const QString &path, const QColor color)
{
    QImage image{96, 64, QImage::Format_ARGB32};
    image.fill(color);
    require(image.save(path));
}
void write(const QString &path, const QByteArray &bytes)
{
    QFile file{path};
    require(file.open(QIODevice::WriteOnly) && file.write(bytes) == bytes.size());
}
} // namespace

int main()
{
    using F = terrain_materials::Family;
    QTemporaryDir temporary;
    require(temporary.isValid());
    const QDir root{temporary.path()};
    require(root.mkpath("user/redhorn-pass") && root.mkpath("assets/redhorn-pass")
            && root.mkpath("assets/lorien") && root.mkpath("legacy/shared"));
    const auto user = root.filePath("user");
    const auto assets = root.filePath("assets");
    save(root.filePath("user/redhorn-pass/terrain-mountain-peaks-seasons.png"), Qt::red);
    save(root.filePath("assets/redhorn-pass/terrain-mountain-peaks-seasons.png"), Qt::blue);
    save(root.filePath("assets/redhorn-pass/terrain-mountain-ground-seasons.png"), Qt::green);
    save(root.filePath("assets/lorien/terrain-forest-ground-seasons.png"), Qt::yellow);
    save(root.filePath("legacy/shared/terrain-field-ground-seasons.png"), Qt::cyan);
    QImage bundled{96, 64, QImage::Format_ARGB32};
    bundled.fill(Qt::white);
    const auto global = [&](const QString &filename) {
        if (filename == QStringLiteral("terrain-city-ground-v2.png")
            || filename == QStringLiteral("terrain-indoor-ground-v2.png")) {
            QImage paving{64, 64, QImage::Format_ARGB32};
            paving.fill(Qt::white);
            return paving;
        }
        return bundled;
    };
    // No manifest file is present: directory discovery alone selects the art.
    const auto manifest = root.filePath("legacy/packs.json");
    auto sprites = terrain_sprites::loadSpritePacks(manifest, bundled, {user, assets});
    const auto packs = terrain_sprites::loadMaterialPacks(sprites, global);
    const auto &pass = packs.find("the Redhorn Pass");
    require(packs.index("the Redhorn Pass") != 0);
    require(packs.index("REDHORN PASS") == packs.index("the Redhorn Pass"));
    require(packs.index("Lórien") == packs.index("Lorien") && packs.index("Lorien") != 0);
    require(packs.index("the Lorien Surroundings") == 0 && packs.index("") == 0);
    const auto color = [&](const F family) {
        return sprites.atlases.at(static_cast<std::size_t>(pass.atlas(family)))
            .front()
            .pixelColor(64, 64);
    };
    require(color(F::MOUNTAIN_PEAK) == QColor{Qt::red});     // User file wins.
    require(color(F::MOUNTAIN_GROUND) == QColor{Qt::green}); // Missing user file inherits assets.
    require(pass.atlas(F::ROAD) == packs.find("unknown").atlas(F::ROAD));
    require(packs.namedAreas.size() == 2); // Repeated folder name shares one pack.

    // A broken higher-priority file falls through without suppressing valid art.
    write(root.filePath("user/redhorn-pass/terrain-mountain-peaks-seasons.png"), "broken PNG");
    write(manifest,
          R"({"superzones":{"Somewhere":"shared","Another":"shared","the Redhorn Pass":"shared"}})");
    write(root.filePath("user/packs.json"), R"({"superzones":{"Elsewhere":"shared"}})");
    auto fallbackSprites = terrain_sprites::loadSpritePacks(manifest, bundled, {user, assets});
    const auto fallback = terrain_sprites::loadMaterialPacks(fallbackSprites, global);
    const auto peak = fallback.find("the Redhorn Pass").atlas(F::MOUNTAIN_PEAK);
    require(fallbackSprites.atlases.at(static_cast<std::size_t>(peak)).front().pixelColor(64, 64)
            == QColor{Qt::blue});
    require(fallback.index("Somewhere") == fallback.index("Another")
            && fallback.index("Somewhere") != 0);
    require(fallback.index("Elsewhere") == fallback.index("Somewhere"));
    require(fallback.find("Somewhere").atlas(F::FIELD_GROUND)
            != fallback.find("unknown").atlas(F::FIELD_GROUND));
}
