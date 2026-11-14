// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 The MMapper Authors
#include "../src/display/TerrainMaterialMaps.h"
#include "../src/display/TerrainReflections.h"
#include "../src/display/TerrainSpritePacks.h"
#include "../src/mainwindow/TerrainPreviewPlayback.h"

#include <cassert>
#include <cmath>
#include <iostream>

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTemporaryDir>

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    using namespace terrain_material_maps;
    QImage source(128, 128, QImage::Format_ARGB32);
    source.fill(qRgba(55, 105, 125, 255));
    for (int y = 28; y < 100; ++y)
        for (int x = 18; x < 112; ++x) {
            const bool ridge = (y % 16) < 5;
            source.setPixel(x, y, ridge ? qRgba(145, 195, 210, 255) : qRgba(45, 85, 110, 255));
        }
    const auto saved = source;
    const auto first = generate(source), repeat = generate(source);
    assert(first == repeat && source == saved);
    bool reflective = false, tilted = false;
    for (int y = 0; y < 128; ++y)
        for (int x = 0; x < 128; ++x) {
            const auto p = first.pixel(x, y);
            reflective |= qAlpha(p) > 50;
            tilted |= std::abs(qGreen(p) - 128) > 5;
            const float nx = static_cast<float>(qRed(p)) / 127.5f - 1.f;
            const float ny = static_cast<float>(qGreen(p)) / 127.5f - 1.f;
            const float nz = static_cast<float>(qBlue(p)) / 127.5f - 1.f;
            assert(std::abs(nx * nx + ny * ny + nz * nz - 1.f) < .025f);
            if (x == 0 || y == 0 || x == 127 || y == 127)
                assert(qAlpha(p) == 0);
        }
    assert(reflective && tilted);
    source.fill(qRgba(255, 255, 255, 0));
    const auto empty = generate(source);
    for (int y = 0; y < 128; ++y)
        for (int x = 0; x < 128; ++x)
            assert(qAlpha(empty.pixel(x, y)) == 0);
    auto colour = neutralPages();
    for (auto &page : colour) {
        page.front().fill(qRgba(55, 105, 125, 255));
        for (int y = 25; y < 110; ++y)
            for (int x = 20; x < 110; ++x)
                page.front().setPixel(x, y, saved.pixel(x, y));
    }
    QTemporaryDir cache;
    assert(cache.isValid());
    const auto generated = loadOrGenerate(colour, cache.path());
    const auto cached = loadOrGenerate(colour, cache.path());
    assert(generated == cached && generated.size() == 2);
    assert(QDir(cache.path()).entryList({"*.png"}, QDir::Files).size() == 2);
    // A corrupt cache is regenerated, never used as random normal vectors.
    QFile corrupt(
        QDir(cache.path()).filePath(QDir(cache.path()).entryList({"*.png"}, QDir::Files).front()));
    assert(corrupt.open(QIODevice::WriteOnly));
    assert(corrupt.write("invalid") == 7);
    corrupt.close();
    assert(loadOrGenerate(colour, cache.path()) == generated);
    colour[0][0].setPixel(64, 64, qRgba(240, 240, 240, 255));
    static_cast<void>(loadOrGenerate(colour, cache.path()));
    assert(QDir(cache.path()).entryList({"*.png"}, QDir::Files).size() == 4);
    // Normal-vector RGB must survive even when reflectivity alpha is zero.
    const auto neutral = neutralPages();
    for (const auto &page : neutral)
        for (const auto &level : page) {
            const auto p = level.pixel(0, 0);
            assert(qRed(p) == 128 && qGreen(p) == 128 && qBlue(p) == 255 && qAlpha(p) == 0);
        }
    // Material-specific masks must select actual pools, foam and snow caps.
    QImage regions(128, 128, QImage::Format_ARGB32);
    for (int y = 0; y < 128; ++y)
        for (int x = 0; x < 128; ++x)
            regions.setPixel(x,
                             y,
                             x < 32   ? qRgba(70, 90, 85, 255)
                             : x < 64 ? qRgba(240, 240, 235, 255)
                             : x < 96 ? qRgba(100, 125, 35, 255)
                                      : qRgba(95, 68, 30, 255));
    const auto pools = generate(regions, Surface::SHALLOWS);
    assert(qAlpha(pools.pixel(16, 64)) > 70);
    assert(qAlpha(pools.pixel(48, 64)) == 0);  // White marks are dry/ice, not pools.
    assert(qAlpha(pools.pixel(80, 64)) == 0);  // Reeds.
    assert(qAlpha(pools.pixel(112, 64)) == 0); // Mud.
    bool texturedPool = false;
    for (int y = 35; y < 95; ++y)
        for (int x = 8; x < 24; ++x)
            texturedPool |= std::abs(qRed(pools.pixel(x, y)) - 128) > 1
                            || std::abs(qGreen(pools.pixel(x, y)) - 128) > 1;
    assert(texturedPool && pools == generate(regions, Surface::SHALLOWS));
    const auto foam = generate(regions, Surface::RAPIDS);
    assert(qAlpha(foam.pixel(48, 64)) > 40);
    assert(qAlpha(foam.pixel(16, 64)) > 40);
    assert(qAlpha(foam.pixel(112, 64)) == 0);
    const auto peaks = generate(regions, Surface::MOUNTAIN);
    assert(qAlpha(peaks.pixel(48, 64)) > 100);
    assert(qAlpha(peaks.pixel(16, 64)) == 0);
    assert(qAlpha(peaks.pixel(80, 64)) == 0);
    assert(qAlpha(peaks.pixel(112, 64)) == 0);
    assert(generate(saved, Surface::WATER) == first); // Original water generation.
    const auto alternate = loadOrGenerate(colour, cache.path(), Surface::MOUNTAIN);
    assert(alternate != loadOrGenerate(colour, cache.path(), Surface::WATER));
    assert(alternate == loadOrGenerate(colour, cache.path(), Surface::MOUNTAIN));
    assert(QDir(cache.path()).entryList({"*.png"}, QDir::Files).size() == 6);
    using terrain_preview::rate;
    assert(std::abs(rate(0) - 1.0) < 1.e-10);
    assert(std::abs(rate(-1) + 1.0) < 1.e-10);
    assert(std::abs(rate(100) - 1024.0) < 1.e-8);
    assert(std::abs(rate(-100) + 1024.0) < 1.e-8);
    for (int i = -100; i <= 100; ++i)
        assert(std::abs(rate(i)) >= 1.0);
    terrain_preview::Playback playback;
    assert(playback.advance(500, 0) == 0);
    assert(playback.advance(500, 0) == 1);
    playback.reset();
    assert(playback.advance(500, -1) == 0);
    assert(playback.advance(500, -1) == -1);
    playback.reset();
    assert(playback.advance(125, 100) == 128);
    const auto morning = terrain_reflections::sample(9, 6, 18, 0, 0, false);
    const auto evening = terrain_reflections::sample(17, 6, 18, 0, 0, false);
    assert(morning.direction[0] > 0.f && evening.direction[0] < 0.f);
    assert(morning.direction[3] > 0.f && evening.direction[3] > 0.f);
    assert(terrain_reflections::sample(2, 6, 18, 0, 0, false).direction[3] <= 0.f);
    const auto moon = terrain_reflections::sample(0, 6, 18, 0, 1, true);
    assert(moon.direction[3] > 0.f && moon.direction[3] < .2f);
    if (argc == 3) {
        const QDir output(QString::fromLocal8Bit(argv[2]));
        const QFileInfo input(QString::fromLocal8Bit(argv[1]));
        const auto exportMaps = [&](const QString &filename,
                                    const QString &name,
                                    Surface surface,
                                    terrain_materials::Placement placement) {
            terrain_sprites::SpritePacks packs;
            const QImage artwork = terrain_sprites::alignSeasonalSheet(QImage(filename), placement);
            if (surface == Surface::RAPIDS)
                static_cast<void>(terrain_sprites::appendFixedSheet(packs, artwork));
            else
                static_cast<void>(terrain_sprites::appendSeasonalSheet(packs, artwork));
            const auto maps = loadOrGenerate(packs.atlases, QString{}, surface);
            const QDir directory(output.filePath(name));
            assert(directory.mkpath("."));
            for (std::size_t page = 0; page < maps.size(); ++page) {
                assert(packs.atlases[page].front().save(
                    directory.filePath(QStringLiteral("colour-%1.png").arg(page))));
                assert(maps[page].front().save(
                    directory.filePath(QStringLiteral("data-%1.png").arg(page))));
            }
        };
        if (input.isDir()) {
            const std::array<const char *, 4> names{"water", "shallows", "rapids", "mountains"};
            for (std::size_t i = 0; i < MATERIALS.size(); ++i) {
                const auto &sheet
                    = terrain_materials::SHEETS[static_cast<std::size_t>(MATERIALS[i].family)];
                exportMaps(QDir(input.filePath()).filePath(QString::fromLatin1(sheet.filename)),
                           QString::fromLatin1(names[i]),
                           MATERIALS[i].surface,
                           sheet.placement);
            }
        } else {
            exportMaps(input.filePath(),
                       QString{},
                       Surface::WATER,
                       terrain_materials::Placement::SOFT_PATCH);
        }
    }
    std::cout << "Material generation, cache, normal data/mips, playback rates and sky light passed\n";
}
