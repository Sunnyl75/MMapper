// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 The MMapper Authors

#include "../src/configuration/configuration.h"
#include "../src/display/BackgroundMapData.h"
#include "../src/display/RoadIndex.h"
#include "../src/global/progresscounter.h"
#include "../src/preferences/BackgroundMapPage.h"

#include <algorithm>
#include <cmath>
#include <stdexcept>

#include <glm/gtc/matrix_transform.hpp>

#include <QApplication>
#include <QCheckBox>
#include <QSettings>
#include <QSlider>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QtTest>

namespace {
ExternalRawRoom room(const uint32_t id,
                     const Coordinate pos,
                     const RoomTerrainEnum terrain = RoomTerrainEnum::FIELD)
{
    ExternalRawRoom result;
    result.id = ExternalRoomId{id};
    result.status = RoomStatusEnum::Permanent;
    result.setPosition(pos);
    result.setTerrainType(terrain);
    return result;
}
} // namespace

class TestBackgroundMap final : public QObject
{
    Q_OBJECT

private slots:
    void coordinatesAndIndependentGeometry()
    {
        ProgressCounter pc;
        auto rooms = std::vector{room(0, Coordinate{-1, -65, 0}), room(1, Coordinate{64, 63, 2})};
        auto geometry = backgroundmap::buildGeometry(rooms, pc);
        QCOMPARE(geometry.size(), size_t{2});
        const auto &negative = geometry.at({0, -1, -2}).front().vertTexCol;
        QCOMPARE(negative.x, -1);
        QCOMPARE(negative.y, -65);
        QCOMPARE(negative.z, 0);
        QCOMPARE(geometry.at({2, 1, 0}).front().vertTexCol.z, 2);
        // A foreground/source object changing does not mutate cached geometry.
        rooms[0].setPosition(Coordinate{500, 500, 0});
        QCOMPARE(geometry.at({0, -1, -2}).front().vertTexCol.x, -1);
    }

    void directionalRoads()
    {
        ProgressCounter pc;
        auto road = room(0, Coordinate{0, 0, 0}, RoomTerrainEnum::ROAD);
        road.getExit(ExitDirEnum::NORTH).setExitFlags(ExitFlagEnum::EXIT | ExitFlagEnum::ROAD);
        road.getExit(ExitDirEnum::EAST).setExitFlags(ExitFlagEnum::EXIT | ExitFlagEnum::ROAD);
        const auto geometry = backgroundmap::buildGeometry({road}, pc);
        const auto mask = getRoadIndex(ExitDirEnum::NORTH) | getRoadIndex(ExitDirEnum::EAST);
        QCOMPARE(geometry.begin()->second.front().vertTexCol.w & 255,
                 backgroundmap::ROAD_TEXTURE_OFFSET + static_cast<int>(mask));
    }

    void cachedBoundsIncludeWholeTilesAndSeparateLayers()
    {
        ProgressCounter pc;
        const auto geometry = backgroundmap::buildGeometry({room(0, Coordinate{-6, -271, 0}),
                                                            room(1, Coordinate{720, 20, 0}),
                                                            room(2, Coordinate{4, 5, -1})},
                                                           pc);
        const auto bounds = backgroundmap::bounds(geometry, pc);
        QCOMPARE(bounds.size(), size_t{2});
        QVERIFY(bounds.at(0).minimum == glm::vec2(-6.f, -271.f));
        QVERIFY(bounds.at(0).maximum == glm::vec2(721.f, 21.f));
        QVERIFY(bounds.at(-1).minimum == glm::vec2(4.f, 5.f));
        QVERIFY(bounds.at(-1).maximum == glm::vec2(5.f, 6.f));
        QVERIFY(backgroundmap::bounds({}, pc).empty());
        pc.requestCancel();
        QVERIFY_THROWS_EXCEPTION(ProgressCanceledException,
                                 static_cast<void>(backgroundmap::bounds(geometry, pc)));
    }

    void rejectsOverlappingRoomsAndHonorsCancellation()
    {
        ProgressCounter pc;
        const auto rooms = std::vector{room(0, Coordinate{1, 1, 0}), room(1, Coordinate{1, 1, 0})};
        QVERIFY_THROWS_EXCEPTION(std::runtime_error,
                                 static_cast<void>(backgroundmap::buildGeometry(rooms, pc)));
        ProgressCounter canceled;
        canceled.requestCancel();
        QVERIFY_THROWS_EXCEPTION(ProgressCanceledException,
                                 static_cast<void>(backgroundmap::buildGeometry(rooms, canceled)));
    }

    void shadowOnlyOnExposedEdges()
    {
        ProgressCounter pc;
        const auto pair = Map::fromRooms(pc,
                                         {room(0, Coordinate{0, 0, 1}),
                                          room(1, Coordinate{1, 0, 1})},
                                         {});
        const auto map = pair.modified;
        const auto shadow = backgroundmap::buildShadow(map, pc);
        QCOMPARE(shadow.size(), size_t{1});
        QCOMPARE(shadow.at(1).size(), size_t{24}); // Six outer edges, no shared-edge seam.
        QVERIFY(std::all_of(shadow.at(1).begin(), shadow.at(1).end(), [](const ColorVert &v) {
            return std::abs(v.vert.z - 1.f) < .001f;
        }));
        QVERIFY(map.isSamePointer(pair.modified));
        QCOMPARE(map.getRoomsCount(), size_t{2});
    }

    void chunkCullingUsesWorldCoordinates()
    {
        const auto projection = glm::ortho(-1.f, 65.f, -1.f, 65.f, -3.f, 3.f);
        QVERIFY(backgroundmap::isChunkVisible({0, 0, 0}, projection));
        QVERIFY(backgroundmap::isChunkVisible({0, -1, -1}, projection));
        QVERIFY(!backgroundmap::isChunkVisible({0, 2, 0}, projection));
        QVERIFY(!backgroundmap::isChunkVisible({4, 0, 0}, projection));
        // Layer 2 is still in view: projection already handles layer spacing.
        QVERIFY(backgroundmap::isChunkVisible({2, 0, 0}, projection));
    }

    void fogShadowsOnlyComeFromKnownRooms()
    {
        ProgressCounter pc;
        auto a = room(0, Coordinate{0, 0, 0});
        auto b = room(1, Coordinate{1, 0, 0});
        a.setServerId(ServerRoomId{1});
        b.setServerId(ServerRoomId{2});
        const auto map = Map::fromRooms(pc, {a, b}, {}).modified;
        RoomVisibility visibility{true, {}};
        QVERIFY(backgroundmap::buildShadow(map, pc, visibility).empty());
        visibility.known.complete = true;
        visibility.known.rooms.insert(ServerRoomId{1});
        const auto one = backgroundmap::buildShadow(map, pc, visibility);
        QCOMPARE(one.at(0).size(), size_t{16});
        visibility.known.rooms.insert(ServerRoomId{2});
        const auto both = backgroundmap::buildShadow(map, pc, visibility);
        QCOMPARE(both.at(0).size(), size_t{24});
        QCOMPARE(map.getRoomsCount(), size_t{2});
    }

    void tileImagesAreCachedAndSofteningIsOptional()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const auto path = directory.filePath("tile.png");
        QImage original(64, 64, QImage::Format_ARGB32);
        original.fill(Qt::red);
        for (int y = 0; y < 64; ++y) {
            for (int x = 32; x < 64; ++x) {
                original.setPixelColor(x, y, Qt::blue);
            }
        }
        QVERIFY(original.save(path));
        ProgressCounter pc;
        const auto sharp = backgroundmap::loadTextures({path}, false, pc);
        const auto soft = backgroundmap::loadTextures({path}, true, pc);
        QCOMPARE(sharp[0].pixelColor(31, 32), QColor(Qt::red));
        QVERIFY(soft[0].pixelColor(31, 32).blue() > 0);
        original.fill(Qt::green);
        QVERIFY(original.save(path));
        QCOMPARE(sharp[0].pixelColor(31, 32), QColor(Qt::red));
        QVERIFY_THROWS_EXCEPTION(std::runtime_error,
                                 static_cast<void>(
                                     backgroundmap::loadTextures({path + "missing"}, false, pc)));
    }

    void opacityDoesNotInvalidateBackgroundContent()
    {
        auto &canvas = setConfig().canvas;
        canvas.backgroundMapOpacity.set(40);
        ChangeMonitor::Lifetime lifetime;
        int opacityChanges = 0;
        int contentChanges = 0;
        canvas.backgroundMapOpacity.registerChangeCallback(lifetime, [&]() { ++opacityChanges; });
        const auto contentChanged = [&]() { ++contentChanges; };
        canvas.backgroundMapFile.registerChangeCallback(lifetime, contentChanged);
        canvas.backgroundMapTileset.registerChangeCallback(lifetime, contentChanged);
        canvas.backgroundMapTilesDirectory.registerChangeCallback(lifetime, contentChanged);
        canvas.backgroundMapSoften.registerChangeCallback(lifetime, contentChanged);
        canvas.backgroundMapReload.registerChangeCallback(lifetime, contentChanged);
        BackgroundMapPage page(nullptr);
        page.slot_loadConfig();
        QCOMPARE(opacityChanges, 0);
        QCOMPARE(contentChanges, 0);
        auto *slider = page.findChild<QSlider *>("backgroundMapOpacity");
        QVERIFY(slider);
        slider->setValue(0);
        QCOMPARE(canvas.backgroundMapOpacity.get(), 0);
        slider->setValue(100);
        QCOMPARE(canvas.backgroundMapOpacity.get(), 100);
        QCOMPARE(opacityChanges, 2);
        QCOMPARE(contentChanges, 0);
    }

    void preferencesRoundTripAndClamp()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        QSettings settings(directory.filePath("settings.ini"), QSettings::IniFormat);
        auto &config = setConfig();
        config.canvas.backgroundMapEnabled.set(true);
        config.canvas.backgroundMapFile.set(QStringLiteral("/maps/background.mm2xml"));
        config.canvas.backgroundMapTileset.set(QStringLiteral("custom"));
        config.canvas.backgroundMapTilesDirectory.set(QStringLiteral("/tiles/background"));
        config.canvas.backgroundMapOpacity.set(37);
        config.canvas.backgroundMapSoften.set(false);
        config.canvas.backgroundMapShadow.set(false);
        config.writeTo(settings);
        config.canvas.backgroundMapFile.set({});
        config.canvas.backgroundMapOpacity.set(0);
        config.readFrom(settings);
        QCOMPARE(config.canvas.backgroundMapFile.get(), QStringLiteral("/maps/background.mm2xml"));
        QCOMPARE(config.canvas.backgroundMapTileset.get(), QStringLiteral("custom"));
        QCOMPARE(config.canvas.backgroundMapOpacity.get(), 37);
        QVERIFY(config.canvas.backgroundMapEnabled.get());
        QVERIFY(!config.canvas.backgroundMapSoften.get());
        QVERIFY(!config.canvas.backgroundMapShadow.get());
        settings.setValue("Canvas/BACKGROUND_MAP_OPACITY", 300);
        settings.setValue("Canvas/BACKGROUND_MAP_TILESET", "invalid");
        config.readFrom(settings);
        QCOMPARE(config.canvas.backgroundMapOpacity.get(), 100);
        QCOMPARE(config.canvas.backgroundMapTileset.get(), QStringLiteral("builtin"));
    }
};

int main(int argc, char **argv)
{
    setEnteredMain();
    QApplication app(argc, argv);
    QStandardPaths::setTestModeEnabled(true);
    QCoreApplication::setOrganizationName(QStringLiteral("MMapperTests"));
    QCoreApplication::setApplicationName(QStringLiteral("BackgroundMap"));
    TestBackgroundMap test;
    return QTest::qExec(&test, argc, argv);
}

#include "TestBackgroundMap.moc"
