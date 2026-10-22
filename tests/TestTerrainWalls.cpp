// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 The MMapper Authors
#include "display/TerrainSpritePacks.h"
#include "display/TerrainWalls.h"

#include <cassert>

#include <QDir>
#include <QPainter>
#include <QString>

int main(int argc, char **argv)
{
    using namespace terrain_walls;
    terrain_sprites::SpritePacks packs;
    for (const auto *name : {"terrain-tunnel-walls.png",
                             "terrain-cavern-walls.png",
                             "terrain-timber-walls.png",
                             "terrain-city-eaves-v3.png",
                             "terrain-stone-walls.png"}) {
        const QImage input{QStringLiteral(TERRAIN_ASSET_DIR "/") + QString::fromLatin1(name)};
        assert(!input.isNull());
        const auto aligned
            = terrain_sprites::alignSeasonalSheet(input,
                                                  QString::fromLatin1(name).contains("eaves")
                                                      ? terrain_materials::Placement::EAVE
                                                  : QString::fromLatin1(name).contains("stone")
                                                      ? terrain_materials::Placement::MASONRY
                                                      : terrain_materials::Placement::WALL);
        assert(!aligned.isNull());
        const int cell = aligned.width() / 6;
        const bool roof = QString::fromLatin1(name).contains("eaves");
        for (int row = 0; row < (roof ? 1 : 4); ++row)
            for (int col = 0; col < 6; ++col) {
                if (!roof)
                    for (int x = 0; x < cell; ++x)
                        assert(aligned.pixelColor(col * cell + x, row * cell).alpha() == 255);
                assert(aligned.pixelColor(col * cell + cell / 2, row * cell + cell / 2).alpha()
                       == 0);
            }
        if (roof) {
            (void) terrain_sprites::appendFixedSheet(packs, aligned);
            for (int col = 0; col < 6; ++col)
                for (int y = 0; y < qRound(cell * 0.24); ++y)
                    assert(aligned.pixel(col * cell, y) == aligned.pixel((col + 1) * cell - 1, y));
        } else {
            (void) terrain_sprites::appendSeasonalSheet(packs, aligned);
        }
    }
    const QImage finishes{QStringLiteral(TERRAIN_ASSET_DIR "/terrain-wall-finishes.png")};
    auto alignedFinishes = terrain_sprites::alignSeasonalSheet(finishes,
                                                               terrain_materials::Placement::FINISH);
    assert(!alignedFinishes.isNull());
    const auto stoneBefore = alignedFinishes.copy(0, 768, 1536, 256);
    const QImage roofCaps{QStringLiteral(TERRAIN_ASSET_DIR "/terrain-roof-caps.png")};
    assert(!roofCaps.isNull());
    terrain_sprites::applyRoofCaps(alignedFinishes, roofCaps);
    assert(stoneBefore == alignedFinishes.copy(0, 768, 1536, 256));
    auto missingCaps = alignedFinishes;
    terrain_sprites::applyRoofCaps(missingCaps, {});
    assert(missingCaps.pixelColor(128, 128).alpha() == 0);
    assert(stoneBefore == missingCaps.copy(0, 768, 1536, 256));
    assert(terrain_sprites::appendFixedSheet(packs, alignedFinishes) == 10);
    // Short source strips repeat at their natural physical proportions.
    QImage shortStrips{1536, 1024, QImage::Format_ARGB32};
    shortStrips.fill(Qt::transparent);
    QPainter painter{&shortStrips};
    for (int row = 0; row < 4; ++row)
        for (int col = 0; col < 6; ++col) {
            painter.fillRect(QRect{col * 256 + 28, row * 256 + 100, 200, 50}, Qt::blue);
            painter.fillRect(QRect{col * 256 + 28, row * 256 + 100, 50, 50}, Qt::red);
        }
    painter.end();
    const auto natural = terrain_sprites::alignSeasonalSheet(shortStrips,
                                                             terrain_materials::Placement::MASONRY);
    assert(natural.pixelColor(20, 25).red() > 200);
    assert(natural.pixelColor(40, 25).blue() > 200);
    assert(natural.pixelColor(120, 25).red() > 200);
    // A roof kit can drift across the nominal 256-pixel columns. Each role
    // must still contain one complete module, never a neighbour's fragment
    // followed by transparent padding (a visible gap on every repetition).
    QImage drifting{1536, 1024, QImage::Format_ARGB32};
    drifting.fill(Qt::transparent);
    QPainter roofPainter{&drifting};
    for (int row = 0; row < 4; ++row)
        for (int col = 0; col < 6; ++col) {
            const int width = row == 0 ? 240 : row == 3 ? 24 : 200;
            const int height = row == 0 ? 70 : 200;
            const QRect module{30 + col * 250, row * 256 + 20, width, height};
            roofPainter.fillRect(module, Qt::blue);
            roofPainter.fillRect(QRect{module.x(), module.y(), width / 4, height}, Qt::red);
        }
    roofPainter.end();
    const auto roofKit = terrain_sprites::alignSeasonalSheet(drifting,
                                                             terrain_materials::Placement::EAVE);
    assert(!roofKit.isNull());
    for (int col = 0; col < 6; ++col) {
        assert(roofKit.pixelColor(col * 256 + 5, 25).red() > 200);
        assert(roofKit.pixelColor(col * 256 + 250, 25).blue() > 200);
        for (int x = 0; x < 256; ++x)
            assert(roofKit.pixelColor(col * 256 + x, 25).alpha() == 255);
    }
    terrain_sprites::SpritePacks fixed;
    QImage roles{48, 32, QImage::Format_ARGB32};
    for (int y = 0; y < 32; ++y)
        for (int x = 0; x < 48; ++x)
            roles.setPixel(x, y, qRgb(40 + 40 * (y / 8), 0, 0));
    assert(terrain_sprites::appendFixedSheet(fixed, roles) == 0);
    for (int role = 0; role < 4; ++role) {
        const int tile = role * 6;
        assert(fixed.atlases[static_cast<size_t>(tile / 16)][0]
                   .pixelColor((tile % 4) * 128 + 10, ((tile % 16) / 4) * 128 + 10)
                   .red()
               == 40 + role * 40);
    }
    if (argc == 2) {
        const QDir output{QString::fromLocal8Bit(argv[1])};
        assert(QDir{}.mkpath(output.absolutePath()));
        for (size_t i = 0; i < packs.atlases.size(); ++i)
            for (size_t level = 0; level < packs.atlases[i].size(); ++level)
                assert(packs.atlases[i][level].save(
                    output.filePath(QStringLiteral("wall-atlas-%1-%2.png").arg(i).arg(level))));
    }
    for (size_t i = 0; i < NUM_ROOM_TERRAIN_TYPES; ++i) {
        const auto terrain = static_cast<RoomTerrainEnum>(i);
        const bool indoor = terrain == RoomTerrainEnum::INDOORS
                            || terrain == RoomTerrainEnum::TUNNEL
                            || terrain == RoomTerrainEnum::CAVERN;
        assert(isIndoors(terrain) == indoor);
        const auto wall = family(terrain);
        assert(wall.has_value()
               == (terrain == RoomTerrainEnum::TUNNEL || terrain == RoomTerrainEnum::CAVERN
                   || terrain == RoomTerrainEnum::INDOORS || terrain == RoomTerrainEnum::CITY));
        if (indoor || terrain == RoomTerrainEnum::UNDERWATER)
            assert(!receivesSkyShadows(terrain, RoomLightEnum::LIT, RoomSundeathEnum::SUNDEATH));
        assert(!receivesSkyShadows(terrain, RoomLightEnum::DARK, RoomSundeathEnum::SUNDEATH));
        assert(!receivesSkyShadows(terrain, RoomLightEnum::LIT, RoomSundeathEnum::NO_SUNDEATH));
    }
    assert(
        receivesSkyShadows(RoomTerrainEnum::FOREST, RoomLightEnum::LIT, RoomSundeathEnum::SUNDEATH));
    assert(receivesSkyShadows(RoomTerrainEnum::FIELD,
                              RoomLightEnum::UNDEFINED,
                              RoomSundeathEnum::UNDEFINED));
    using F = terrain_materials::Family;
    assert(family(RoomTerrainEnum::CITY, true) == F::CITY_EAVE);
    assert(family(RoomTerrainEnum::CITY) == F::STONE_WALL);
    assert(family(RoomTerrainEnum::FIELD, true) == F::STONE_WALL);
    assert(family(RoomTerrainEnum::FIELD, false, true) == F::STONE_WALL);
    assert(family(RoomTerrainEnum::INDOORS, false, true) == F::TIMBER_WALL);
    assert(!family(RoomTerrainEnum::UNDERWATER, true));
    // A deliberately nonperiodic source must join even at coarse mip levels.
    QImage nonperiodic{64, 64, QImage::Format_ARGB32};
    for (int y = 0; y < 64; ++y)
        for (int x = 0; x < 64; ++x)
            nonperiodic.setPixel(x, y, qRgb(x * 4, y * 4, 90));
    terrain_sprites::SpritePacks repeating;
    assert(terrain_sprites::appendRepeatMaterial(repeating, nonperiodic) == 0);
    assert(repeating.atlases.size() == 2);
    for (const auto &page : repeating.atlases) {
        assert(page.size() == 8);
        for (const auto &mip : page) {
            for (int x = 0; x < mip.width(); ++x)
                assert(mip.pixel(x, 0) == mip.pixel(x, mip.height() - 1));
            for (int y = 0; y < mip.height(); ++y)
                assert(mip.pixel(0, y) == mip.pixel(mip.width() - 1, y));
        }
    }
    // A doorway end, a straight continuation and a turned corner are distinct.
    // Actual underwater/door art uses fixed roles in compact source grids.
    for (const auto *name :
         {"terrain-underwater-ground.png", "terrain-underwater-details.png", "terrain-doors.png"}) {
        const QImage input{QStringLiteral(TERRAIN_ASSET_DIR "/") + QString::fromLatin1(name)};
        const bool ground = QString::fromLatin1(name).contains("ground");
        const auto expanded = terrain_sprites::expandFixedSheet(input, ground ? 3 : 6, 2);
        assert(!expanded.isNull());
        const auto aligned
            = terrain_sprites::alignSeasonalSheet(expanded,
                                                  ground ? terrain_materials::Placement::PATCH
                                                  : QString::fromLatin1(name).contains("doors")
                                                      ? terrain_materials::Placement::MATERIAL
                                                      : terrain_materials::Placement::UPRIGHT);
        assert(!aligned.isNull());
        terrain_sprites::SpritePacks fixedArt;
        assert(terrain_sprites::appendFixedSheet(fixedArt, aligned) == 0);
        assert(fixedArt.atlases.size() == 2);
    }
    const QImage doorSource{QStringLiteral(TERRAIN_ASSET_DIR "/terrain-doors.png")};
    const auto doors = terrain_sprites::alignDoorSheet(doorSource);
    assert(!doors.isNull());
    for (int style = 0; style < 6; ++style) {
        const auto closedDoor = doors.copy(style * 256, 0, 256, 256);
        const auto openDoor = doors.copy(style * 256, 256, 256, 256);
        int retained = 0, removed = 0;
        for (int y = 0; y < 256; ++y)
            for (int x = 0; x < 256; ++x) {
                const auto original = closedDoor.pixel(x, y);
                const auto opened = openDoor.pixel(x, y);
                assert(qAlpha(opened) == 0 || opened == original);
                retained += qAlpha(opened) > 128;
                removed += qAlpha(original) > 128 && qAlpha(opened) == 0;
                if (x >= 112 && x < 144)
                    assert(qAlpha(opened) == 0); // Clear opening, no swung leaf.
            }
        assert(retained > 100 && removed > 100);
        assert(doors.copy(style * 256, 512, 256, 256) == closedDoor);
        assert(doors.copy(style * 256, 768, 256, 256) == openDoor);
    }
    terrain_sprites::SpritePacks doorPacks;
    assert(terrain_sprites::appendFixedSheet(doorPacks, doors) == 0);
    const QImage danger{QStringLiteral(TERRAIN_ASSET_DIR "/terrain-death-trap.png")};
    assert(!danger.isNull());
    assert(!terrain_sprites::expandFixedSheet(danger, 1, 1).isNull());
    terrain_sprites::SpritePacks warningPacks;
    assert(terrain_sprites::appendFixedSheet(warningPacks,
                                             terrain_sprites::expandFixedSheet(danger, 1, 1))
           == 0);
    if (argc == 2)
        for (std::size_t page = 0; page < warningPacks.atlases.size(); ++page)
            for (std::size_t level = 0; level < warningPacks.atlases[page].size(); ++level)
                assert(warningPacks.atlases[page][level].save(
                    QString::fromLocal8Bit(argv[1])
                    + QStringLiteral("/warning-atlas-%1-%2.png").arg(page).arg(level)));
    if (argc == 2)
        for (std::size_t page = 0; page < doorPacks.atlases.size(); ++page)
            for (std::size_t level = 0; level < doorPacks.atlases[page].size(); ++level)
                assert(doorPacks.atlases[page][level].save(
                    QString::fromLocal8Bit(argv[1])
                    + QStringLiteral("/door-atlas-%1-%2.png").arg(page).arg(level)));
    assert(ends(0, 0) == 3);                     // isolated north segment: two caps
    assert(ends(0, (1u << 3) | (1u << 2)) == 0); // west/east continuation: no posts
    assert(ends(0, 1u << 3) == 2);               // east doorway end only
    assert(openEnds(0, 3, 1u << 3) == 2);        // west corner keeps its grime
    assert(openEnds(0, 3, (1u << 3) | (1u << 2)) == 0);
    assert((alongDirections(0) == std::array<unsigned, 2>{3, 2}));
    assert((alongDirections(1) == std::array<unsigned, 2>{2, 3}));
    assert((alongDirections(2) == std::array<unsigned, 2>{0, 1}));
    assert((alongDirections(3) == std::array<unsigned, 2>{1, 0}));
    static_assert(WALL_HEIGHT_ROOMS == 0.5f);
    assert(shadowOffsets.front()[0] == -2 && shadowOffsets.back()[1] == 2);
    assert(rotation(0) == 0 && rotation(1) == 2 && rotation(2) == 3 && rotation(3) == 1);
    // 44 regional tree packs + fallback and all shared families fit the existing
    // portable 256-layer minimum; only the first shared tile is packed in eleven bits.
    static_assert(90 + terrain_materials::SHEETS.size() * terrain_materials::LAYERS_PER_SHEET
                  <= 256);
    static_assert(90 * 16 < 2048);
}
