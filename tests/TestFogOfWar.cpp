// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 The MMapper Authors

#include "../src/configuration/configuration.h"
#include "../src/display/RoomVisibility.h"
#include "../src/global/progresscounter.h"
#include "../src/observer/gameobserver.h"
#include "../src/preferences/FogOfWarPage.h"

#include <QApplication>
#include <QCheckBox>
#include <QSettings>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QtTest>

class TestFogOfWar final : public QObject
{
    Q_OBJECT

private slots:
    void strictRoomVisibility()
    {
        ProgressCounter pc;
        std::vector<ExternalRawRoom> rooms;
        for (uint32_t n = 0; n < 3; ++n) {
            ExternalRawRoom room;
            room.id = ExternalRoomId{n};
            room.status = RoomStatusEnum::Permanent;
            room.setPosition(Coordinate{static_cast<int>(n), 0, 0});
            room.setServerId(n == 2 ? INVALID_SERVER_ROOMID : ServerRoomId{n + 1});
            rooms.push_back(room);
        }
        const auto map = Map::fromRooms(pc, rooms, {}).modified;
        const auto unchanged = map;
        RoomVisibility visibility{true, {}};
        map.getRooms().for_each(
            [&](const RoomId id) { QVERIFY(!visibility.allows(map.getRoomHandle(id))); });
        visibility.known.rooms.insert(ServerRoomId{1});
        QVERIFY(!visibility.allows(map.findRoomHandle(ServerRoomId{1})));
        visibility.known.complete = true;
        QVERIFY(visibility.allows(map.findRoomHandle(ServerRoomId{1})));
        QVERIFY(!visibility.allows(map.findRoomHandle(ServerRoomId{2})));
        map.getRooms().for_each([&](const RoomId id) {
            const auto room = map.getRoomHandle(id);
            if (room.getServerId() == INVALID_SERVER_ROOMID) {
                QVERIFY(!visibility.allows(room));
            }
        });
        visibility.enabled = false;
        map.getRooms().for_each(
            [&](const RoomId id) { QVERIFY(visibility.allows(map.getRoomHandle(id))); });
        QVERIFY(map.isSamePointer(unchanged));
        QCOMPARE(map.getRoomsCount(), size_t{3});
    }

    void liveGrowthAndReplacement()
    {
        GameObserver observer;
        observer.setKnownRoomsConsumerEnabled(QStringLiteral("fog-test"), true);
        const auto receive = [&](const QByteArray &message) {
            observer.observeSentToUserGmcp(GmcpMessage::fromRawBytes(message));
        };
        receive(R"(Char.Name {"name":"Bilbo"})");
        receive("Room.Known.List [1]");
        receive("Room.Known.List null");
        const RoomVisibility rendered{true, observer.getKnownRooms()};
        receive("Room.Known.Add 2");
        const RoomVisibility grown{true, observer.getKnownRooms()};
        QVERIFY(!rendered.matches(grown));
        QVERIFY(rendered.canDisplayFor(grown));
        QVERIFY(!rendered.known.isKnown(ServerRoomId{2}));
        QVERIFY(grown.known.isKnown(ServerRoomId{2}));
        receive("Room.Known.Updated");
        QVERIFY(!rendered.canDisplayFor(RoomVisibility{true, observer.getKnownRooms()}));
        receive("Room.Known.List [2]");
        receive("Room.Known.List null");
        const RoomVisibility replacement{true, observer.getKnownRooms()};
        QVERIFY(!rendered.canDisplayFor(replacement));
        QVERIFY(!replacement.known.isKnown(ServerRoomId{1}));
        receive(R"(Char.Name {"name":"Frodo"})");
        QVERIFY(!replacement.canDisplayFor(RoomVisibility{true, observer.getKnownRooms()}));
    }

    void togglingCannotReuseUnrestrictedGeometry()
    {
        const RoomVisibility unrestricted;
        RoomVisibility restricted{true, {}};
        QVERIFY(!unrestricted.hidesAllRooms());
        QVERIFY(restricted.hidesAllRooms());
        restricted.known.complete = true;
        QVERIFY(restricted.hidesAllRooms());
        restricted.known.rooms.insert(ServerRoomId{1});
        QVERIFY(!restricted.hidesAllRooms());
        QVERIFY(!unrestricted.canDisplayFor(restricted));
        QVERIFY(restricted.canDisplayFor(unrestricted));
        QVERIFY(!restricted.matches(unrestricted));
        RoomVisibility irrelevant = unrestricted;
        irrelevant.known.revision = 99;
        QVERIFY(unrestricted.matches(irrelevant));
    }

    void preferencesPersistAndReloadWithoutSideEffects()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        auto &config = setConfig();
        config.canvas.fogOfWarEnabled.set(false);
        FogOfWarPage page(nullptr);
        auto *checkbox = page.findChild<QCheckBox *>("fogOfWarEnabled");
        QVERIFY(checkbox);
        QVERIFY(!checkbox->isChecked());
        checkbox->setChecked(true);
        QVERIFY(config.canvas.fogOfWarEnabled.get());
        QSettings settings(directory.filePath("fog.ini"), QSettings::IniFormat);
        config.writeTo(settings);
        config.canvas.fogOfWarEnabled.set(false);
        config.readFrom(settings);
        QVERIFY(config.canvas.fogOfWarEnabled.get());
        page.slot_loadConfig();
        QVERIFY(checkbox->isChecked());
        config.canvas.fogOfWarEnabled.set(false);
    }
};

int main(int argc, char **argv)
{
    setEnteredMain();
    QApplication app(argc, argv);
    QStandardPaths::setTestModeEnabled(true);
    QCoreApplication::setOrganizationName(QStringLiteral("MMapperTests"));
    QCoreApplication::setApplicationName(QStringLiteral("FogOfWar"));
    TestFogOfWar test;
    return QTest::qExec(&test, argc, argv);
}

#include "TestFogOfWar.moc"
