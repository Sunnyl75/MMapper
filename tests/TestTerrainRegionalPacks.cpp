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
        throw std::runtime_error("Regional terrain pack regression");
}
void write(const QString &path, const QByteArray &data)
{
    QFile file{path};
    require(file.open(QIODevice::WriteOnly));
    require(file.write(data) == data.size());
}
} // namespace

int main()
{
    using F = terrain_materials::Family;
    const QDir assets{QString::fromUtf8(TERRAIN_ASSET_DIR)};
    const auto image = [&](const QString &filename) { return QImage{assets.filePath(filename)}; };
    const auto bundled = image(QStringLiteral("terrain-trees-seasons.png"));
    require(!bundled.isNull());
    QTemporaryDir temporary;
    require(temporary.isValid());
    const QDir root{temporary.path()};
    require(root.mkpath(QStringLiteral("regional")));
    const auto manifest = root.filePath(QStringLiteral("packs.json"));
    write(manifest, R"({"superzones":{"Lorien":"regional","Alias":"regional","Missing":"absent"}})");
    const auto local = root.filePath(QStringLiteral("regional/"));
    // Exercise each actual native sheet format, including repeat paving,
    // doors, caps, fixed water overlays and seasonal ground/foreground art.
    for (const auto &sheet : terrain_materials::SHEETS)
        require(QFile::copy(assets.filePath(QString::fromLatin1(sheet.filename)),
                            local + QString::fromLatin1(sheet.filename)));
    require(QFile::copy(assets.filePath(QStringLiteral("terrain-roof-caps.png")),
                        local + QStringLiteral("terrain-roof-caps.png")));
    auto sprites = terrain_sprites::loadSpritePacks(manifest, bundled);
    const auto selections = terrain_sprites::loadMaterialPacks(sprites, image);
    require(selections.packs.size() == 2);
    require(selections.index("Lorien") == selections.index("Alias"));
    require(selections.index("Missing") == 0 && selections.index("lorien") == 0);
    const auto &region = selections.find("Lorien");
    const auto &defaults = selections.find("other region");
    for (std::size_t i = 0; i < terrain_materials::SHEETS.size(); ++i) {
        const auto family = static_cast<F>(i);
        require(region.has(family));
        require(region.atlas(family) != defaults.atlas(family));
    }
    const int dataBase = static_cast<int>(sprites.atlases.size());
    const auto lookup = terrain_sprites::packLookup(sprites,
                                                    selections,
                                                    {{region.atlas(F::WATER), dataBase, 0}});
    require(lookup.size() == 8);
    const int row = selections.index("Lorien");
    const auto entry = lookup.front().pixelColor(static_cast<int>(F::FIELD_DETAIL), row);
    require(entry.red() + entry.green() * 256 == region.atlas(F::FIELD_DETAIL) * 16);
    const auto reflected = lookup.front().pixelColor(region.atlas(F::WATER), 64);
    require(reflected.red() + reflected.green() * 256 == dataBase * 16);
    require(reflected.blue() == 1);
    require(lookup.front().pixelColor(region.atlas(F::WATER) + 1, 64) == reflected);
    require(lookup.front().pixelColor(32, row).alpha() == 255); // Regional undercoat.

    // Invalid and absent regional art inherit the normal family, rather than
    // suppressing an unrelated overlay or manufacturing placeholder artwork.
    const auto forestFile = local + QStringLiteral("terrain-forest-ground-seasons.png");
    write(forestFile, "invalid PNG");
    require(QFile::remove(local + QStringLiteral("terrain-road-seasons.png")));
    require(QFile::remove(local + QStringLiteral("terrain-field-details-seasons.png")));
    QImage flowers{96, 64, QImage::Format_ARGB32};
    flowers.fill(QColor{200, 170, 40});
    require(flowers.save(local + QStringLiteral("terrain-field-details-all.png")));
    auto fallbackSprites = terrain_sprites::loadSpritePacks(manifest, bundled);
    const auto fallback = terrain_sprites::loadMaterialPacks(fallbackSprites, image);
    require(fallback.find("Lorien").atlas(F::FOREST_GROUND)
            == fallback.find("unknown").atlas(F::FOREST_GROUND));
    require(fallback.find("Lorien").atlas(F::ROAD) == fallback.find("unknown").atlas(F::ROAD));
    const int flowerBase = fallback.find("Lorien").atlas(F::FIELD_DETAIL);
    require(flowerBase != fallback.find("unknown").atlas(F::FIELD_DETAIL));
    for (int season = 0; season < 4; ++season) {
        const int index = season * 6;
        const auto &page
            = fallbackSprites.atlases.at(static_cast<std::size_t>(flowerBase + index / 16)).front();
        require(page.pixelColor(index % 4 * 128 + 64, index % 16 / 4 * 128 + 64)
                == QColor(200, 170, 40));
    }
    // A low-limit device retains valid global art without overflowing its
    // array; reflection/data/lookup reservations are included in the budget.
    auto limitedSprites = terrain_sprites::loadSpritePacks(manifest, bundled);
    const int limit = static_cast<int>(limitedSprites.atlases.size()) + 64 + 9;
    const auto limited = terrain_sprites::loadMaterialPacks(limitedSprites, image, limit);
    require(static_cast<int>(limitedSprites.atlases.size()) + 9 == limit);
    for (const auto &selection : limited.packs)
        require(selection.materials == limited.packs.front().materials);
}
