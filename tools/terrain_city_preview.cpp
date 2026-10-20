// SPDX-License-Identifier: GPL-2.0-or-later
// Private screenshot fixture: link in place of main.cpp, using the real app.
#include "clock/mumeclock.h"
#include "configuration/configuration.h"
#include "display/MapCanvasWindow.h"
#include "display/ProjectionUtils.h"
#include "display/mapwindow.h"
#include "global/progresscounter.h"
#include "mainwindow/mainwindow.h"
#include "map/RawRoom.h"
#include "mapdata/mapdata.h"
#include "mapstorage/RawMapData.h"
#include "opengl/OpenGLConfig.h"

#include <algorithm>
#include <array>
#include <cassert>
#include <vector>

#include <QApplication>
#include <QDir>
#include <QDockWidget>
#include <QFile>
#include <QFileInfo>
#include <QPainter>
#include <QScreen>
#include <QSurfaceFormat>
#include <QTimer>

namespace {
bool built(int x, int y)
{
    return x >= 3 && x <= 14 && y >= 3 && y <= 12;
}
bool block(int x, int y, int x0, int x1, int y0, int y1)
{
    return x >= x0 && x <= x1 && y >= y0 && y <= y1;
}
RoomTerrainEnum terrain(int x, int y)
{
    if (block(x, y, 3, 5, 8, 10) || block(x, y, 3, 5, 3, 5)
        || (block(x, y, 8, 10, 3, 5) && !(x == 10 && y == 3)) || block(x, y, 12, 14, 8, 10)
        || block(x, y, 8, 10, 11, 12) || block(x, y, 12, 14, 3, 5))
        return RoomTerrainEnum::INDOORS;
    if (built(x, y) || block(x, y, 6, 7, 1, 13) || block(x, y, 1, 16, 6, 7))
        return RoomTerrainEnum::CITY;
    return RoomTerrainEnum::FIELD;
}
bool wall(int x, int y, int nx, int ny)
{
    if (nx < 0 || nx >= 18 || ny < 0 || ny >= 14)
        return false;
    std::array<int, 4> edge{x, y, nx, ny};
    if (edge[0] > edge[2] || (edge[0] == edge[2] && edge[1] > edge[3]))
        edge = {nx, ny, x, y};
    constexpr std::array<std::array<int, 4>, 6> doors{
        {{5, 9, 6, 9}, {5, 4, 6, 4}, {7, 4, 8, 4}, {11, 9, 12, 9}, {9, 10, 9, 11}, {13, 5, 13, 6}}};
    if (std::find(doors.begin(), doors.end(), edge) != doors.end())
        return false;
    if (edge[0] == 4 && edge[2] == 5 && edge[1] >= 8 && edge[1] <= 9)
        return true;
    if (edge[1] == 8 && edge[3] == 9 && (edge[0] == 12 || edge[0] == 14))
        return true;
    auto a = terrain(x, y), b = terrain(nx, ny);
    if ((a == RoomTerrainEnum::INDOORS) != (b == RoomTerrainEnum::INDOORS))
        return true;
    return built(x, y) != built(nx, ny)
           && (a == RoomTerrainEnum::FIELD || b == RoomTerrainEnum::FIELD);
}
} // namespace
int main(int argc, char **argv)
{
    setEnteredMain();
    QSurfaceFormat fmt;
    fmt.setVersion(3, 3);
    fmt.setProfile(QSurfaceFormat::CoreProfile);
    fmt.setDepthBufferSize(24);
    QSurfaceFormat::setDefaultFormat(fmt);
    QApplication app(argc, argv);
    app.setOrganizationName("MMapperTerrainPreview");
    app.setApplicationName("CityPreview");
    OpenGLConfig::setBackendType(OpenGLProber::BackendType::GL);
    auto &cfg = setConfig();
    cfg.general.firstRun = false;
    cfg.general.checkForUpdate = false;
    cfg.general.mapMode = MapModeEnum::OFFLINE;
    cfg.canvas.organicTerrain.set(true);
    cfg.canvas.seasonalTerrain.set(false);
    cfg.canvas.showMissingMapId.set(false);
    cfg.canvas.showUnsavedChanges.set(false);
    cfg.canvas.showUnmappedExits.set(false);
    cfg.canvas.advanced.use3D.set(false);
    cfg.canvas.advanced.autoTilt.set(false);
    cfg.canvas.advanced.printPerfStats.set(false);
    cfg.canvas.weatherAtmosphereIntensity.set(0);
    cfg.canvas.weatherPrecipitationIntensity.set(0);
    cfg.canvas.weatherTimeOfDayIntensity.set(0);
    // Verify that captures use the current artwork/shader, rather than an
    // older resource object whose timestamp happened to survive a cached build.
    const QDir resources{QFileInfo{QString::fromUtf8(__FILE__)}.dir().filePath("../src/resources")};
    for (const auto *name : {"pixmaps/terrain-wall-finishes.png",
                             "pixmaps/terrain-roof-caps.png",
                             "pixmaps/terrain-city-eaves-v3.png",
                             "pixmaps/terrain-stone-walls.png",
                             "shaders/legacy/terrain/frag.glsl"}) {
        QFile local{resources.filePath(QString::fromLatin1(name))};
        QFile bundled{QStringLiteral(":/") + QString::fromLatin1(name)};
        assert(local.open(QIODevice::ReadOnly) && bundled.open(QIODevice::ReadOnly));
        assert(local.readAll() == bundled.readAll());
    }
    MainWindow window;
    window.resize(1480, 1080);
    for (auto *dock : window.findChildren<QDockWidget *>())
        dock->hide();
    auto *data = window.findChild<MapData *>("MapData");
    assert(data);
    std::vector<ExternalRawRoom> rooms;
    constexpr std::array<std::array<int, 2>, 4> delta{{{0, 1}, {0, -1}, {1, 0}, {-1, 0}}};
    for (int y = 0; y < 14; ++y)
        for (int x = 0; x < 18; ++x) {
            ExternalRawRoom room;
            room.id = ExternalRoomId{static_cast<uint32_t>(1 + y * 18 + x)};
            room.status = RoomStatusEnum::Permanent;
            room.position = Coordinate{x, y, 0};
            room.setTerrainType(terrain(x, y));
            room.setLightType(RoomLightEnum::LIT);
            room.setSundeathType(RoomSundeathEnum::SUNDEATH);
            room.setArea(RoomArea{"Terrain preview"});
            room.setName(RoomName{"City terrain demonstration"});
            for (unsigned i = 0; i < 4; ++i) {
                int nx = x + delta[i][0], ny = y + delta[i][1];
                if (wall(x, y, nx, ny))
                    continue;
                auto &exit = room.exits[static_cast<ExitDirEnum>(i)];
                exit.setExitFlags(ExitFlags{ExitFlagEnum::EXIT});
                if (nx >= 0 && nx < 18 && ny >= 0 && ny < 14) {
                    const ExternalRoomId id{static_cast<uint32_t>(1 + ny * 18 + nx)};
                    exit.outgoing.insert(id);
                    exit.incoming.insert(id);
                }
            }
            rooms.push_back(std::move(room));
        }
    ProgressCounter progress;
    MapLoadData load;
    load.mapPair = Map::fromRooms(progress, std::move(rooms), {});
    load.position = Coordinate{0, 0, 0};
    data->setMapData(load);
    data->checkSize();
    auto *mapWindow = window.findChild<MapWindow *>();
    assert(mapWindow);
    mapWindow->hideSplashImage();
    auto *canvas = mapWindow->getCanvas();
    assert(canvas);
    window.show();
    canvas->slot_dataLoaded();
    canvas->setZoom(1.4f);
    canvas->slot_setScroll({9.f, 7.f});
    canvas->slot_mapChanged();
    QDir{}.mkpath("terrain-preview");
    QTimer::singleShot(4500, [&] {
        auto *clock = window.findChild<MumeClock *>();
        assert(clock);
        clock->parseMumeTime("3 pm on Highday, the 18th of Halimath, year 3030 of the Third Age.");
        clock->slot_tick();
        clock->slot_tick();
        canvas->setZoom(1.4f);
        canvas->slot_setScroll({9.f, 7.f});
        canvas->slot_requestUpdate();
    });
    QTimer::singleShot(16000, [&] {
        auto image = canvas->grabFramebuffer();
        assert(!image.isNull());
        assert(image.save("terrain-preview/mmapper-city-overview.png"));
        assert(
            window.screen()->grabWindow(window.winId()).save("terrain-preview/mmapper-city-app.png"));
        const auto matrix = ProjectionUtils::calculateViewProjOld({9.f, 7.f},
                                                                  {image.width(), image.height()},
                                                                  canvas->getTotalScaleFactor(),
                                                                  0);
        const auto project = [&](float x, float y) {
            auto p = matrix * glm::vec4{x, y, 0.f, 1.f};
            return QPointF{(static_cast<double>(p.x / p.w) + 1.0) * image.width() / 2.0,
                           (1.0 - static_cast<double>(p.y / p.w)) * image.height() / 2.0};
        };
        const double sx = project(1, 0).x() - project(0, 0).x(),
                     sy = project(0, 0).y() - project(0, 1).y();
        assert(std::abs(sx - sy) < 0.01);
        qInfo() << "Square room grid:" << sx << sy;
        QPainter painter{&image};
        painter.setPen(QPen{QColor{50, 240, 255, 170}, 1});
        for (int x = 0; x <= 18; ++x)
            painter.drawLine(project(static_cast<float>(x), 0), project(static_cast<float>(x), 14));
        for (int y = 0; y <= 14; ++y)
            painter.drawLine(project(0, static_cast<float>(y)), project(18, static_cast<float>(y)));
        painter.fillRect(QRect{15, 15, 700, 38}, QColor{15, 20, 25, 225});
        painter.setPen(Qt::white);
        painter
            .drawText(28,
                      40,
                      QStringLiteral(
                          "DIAGNOSTIC ONLY: square rooms %1 x %2 pixels; cyan grid is not in the app")
                          .arg(sx, 0, 'f', 1)
                          .arg(sy, 0, 'f', 1));
        painter.end();
        assert(image.save("terrain-preview/mmapper-city-grid-diagnostic.png"));
        canvas->setZoom(2.8f);
        canvas->slot_setScroll({6.8f, 8.8f});
        canvas->slot_requestUpdate();
    });
    QTimer::singleShot(19000, [&] {
        assert(canvas->grabFramebuffer().save("terrain-preview/mmapper-city-detail.png"));
        canvas->setZoom(5.6f);
        canvas->slot_setScroll({5.5f, 10.1f});
        canvas->slot_requestUpdate();
    });
    QTimer::singleShot(22000, [&] {
        assert(canvas->grabFramebuffer().save("terrain-preview/mmapper-city-materials-closeup.png"));
        canvas->setZoom(3.8f);
        canvas->slot_setScroll({9.6f, 4.4f});
        canvas->slot_requestUpdate();
    });
    QTimer::singleShot(25000, [&] {
        assert(canvas->grabFramebuffer().save("terrain-preview/mmapper-roof-assembly.png"));
        app.quit();
    });
    return app.exec();
}
