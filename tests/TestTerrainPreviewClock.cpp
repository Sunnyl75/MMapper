// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 The MMapper Authors
#include "../src/clock/mumeclock.h"
#include "../src/display/TerrainTwilight.h"
#include "../src/mainwindow/TerrainPreviewClock.h"

#include <array>
#include <cassert>
#include <iostream>
#include <memory>
#include <optional>
#include <vector>

#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QDockWidget>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QTest>
#include <QTimer>

int main(int argc, char **argv)
{
    QApplication app{argc, argv};
    std::vector<std::optional<MumeMoment>> seen;
    std::unique_ptr<QDockWidget> dock{
        createTerrainPreviewClock(nullptr, [&](auto moment) { seen.push_back(moment); })};
    dock->show();
    QApplication::processEvents();
    auto *enabled = dock->findChild<QCheckBox *>("TerrainPreviewClockEnabled");
    const auto field = [&](const char *name) {
        auto *spin = dock->findChild<QSpinBox *>(QStringLiteral("TerrainPreviewClock")
                                                 + QString::fromLatin1(name));
        assert(spin);
        return spin;
    };
    auto *year = field("Year"), *month = field("Month"), *day = field("Day"), *hour = field("Hour"),
         *minute = field("Minute");
    auto *timer = dock->findChild<QTimer *>("TerrainPreviewClockTimer");
    assert(enabled && timer);
    timer->stop();
    enabled->setChecked(true);
    const auto set = [&](int y, int mo, int d, int h, int mi) {
        const std::array<QSignalBlocker, 5> blockers{QSignalBlocker{year},
                                                     QSignalBlocker{month},
                                                     QSignalBlocker{day},
                                                     QSignalBlocker{hour},
                                                     QSignalBlocker{minute}};
        year->setValue(y);
        month->setValue(mo);
        day->setValue(d);
        hour->setValue(h);
        minute->setValue(mi);
    };
    const auto expect = [&](int y, int mo, int d, int h, int mi) {
        assert(year->value() == y && month->value() == mo && day->value() == d && hour->value() == h
               && minute->value() == mi);
        assert(seen.back());
        const auto &m = *seen.back();
        assert(m.year == y && m.month == mo - 1 && m.day == d - 1 && m.hour == h && m.minute == mi);
    };
    set(2900, 12, 30, 23, 59);
    const auto before = seen.size();
    minute->stepUp();
    expect(2901, 1, 1, 0, 0);
    assert(seen.size() == before + 1);
    minute->stepDown();
    expect(2900, 12, 30, 23, 59);
    set(2900, 5, 12, 23, 10);
    hour->stepUp();
    expect(2900, 5, 13, 0, 10);
    hour->stepDown();
    expect(2900, 5, 12, 23, 10);
    set(2900, 12, 30, 12, 10);
    day->stepUp();
    expect(2901, 1, 1, 12, 10);
    day->stepDown();
    expect(2900, 12, 30, 12, 10);
    set(2900, 12, 15, 12, 10);
    month->stepUp();
    expect(2901, 1, 15, 12, 10);
    month->stepDown();
    expect(2900, 12, 15, 12, 10);
    set(2900, 11, 15, 12, 33);
    QTest::keyClick(month, Qt::Key_PageUp);
    expect(2901, 9, 15, 12, 33);
    QTest::keyClick(month, Qt::Key_PageDown);
    expect(2900, 11, 15, 12, 33);
    set(MUME_START_YEAR, 1, 1, 0, 0);
    minute->stepDown();
    expect(MUME_START_YEAR, 1, 1, 0, 0);
    set(4099, 12, 30, 23, 59);
    minute->stepUp();
    expect(4099, 12, 30, 23, 59);
    set(2900, 5, 12, 23, 10);
    hour->setValue(0);
    expect(2900, 5, 12, 0, 10); // typing/setValue is an edit, not a wrap
    set(2900, 12, 30, 23, 22);
    auto *speed = dock->findChild<QComboBox *>("TerrainPreviewClockSpeed");
    assert(speed);
    speed->setCurrentIndex(speed->findData(60));
    assert(QMetaObject::invokeMethod(timer, "timeout", Qt::DirectConnection));
    expect(2901, 1, 1, 0, 22);
    // Displayed time is the actual MumeMoment time, with no one-hour shift.
    set(2900, 1, 15, 8, 30);
    minute->stepUp();
    expect(2900, 1, 15, 8, 31);
    const terrain_twilight::Tint night{0.05f, 0.05f, 0.2f, 0.35f};
    for (int m = 0; m < MUME_MONTHS_PER_YEAR; ++m) {
        const auto schedule = MumeClock::getDawnDusk(m);
        const auto at = [&](const int h) {
            set(2900, m + 1, 15, h, 0);
            minute->stepUp();
            expect(2900, m + 1, 15, h, 1);
            return *seen.back();
        };
        const auto colour = [&](const MumeMoment &moment) {
            return terrain_twilight::sample(moment.hour + moment.minute / 60.0,
                                            schedule.dawnHour,
                                            schedule.duskHour,
                                            night);
        };
        const auto beforeDawn = at(schedule.dawnHour - 1);
        assert(beforeDawn.toTimeOfDay() == MumeTimeEnum::NIGHT);
        assert(!colour(beforeDawn).changing);
        assert(std::abs(colour(beforeDawn).tint[3] - night[3]) < 1.e-6f);
        const auto dawn = at(schedule.dawnHour);
        assert(dawn.toTimeOfDay() == MumeTimeEnum::DAWN);
        assert(colour(dawn).changing && colour(dawn).tint[3] > 0.f);
        const auto daylightMoment = at(schedule.dawnHour + 1);
        assert(daylightMoment.toTimeOfDay() == MumeTimeEnum::DAY);
        assert(!colour(daylightMoment).changing);
        assert(std::abs(colour(daylightMoment).tint[3]) < 1.e-6f);
        const auto beforeDusk = at(schedule.duskHour - 1);
        assert(beforeDusk.toTimeOfDay() == MumeTimeEnum::DAY);
        assert(!colour(beforeDusk).changing);
        assert(std::abs(colour(beforeDusk).tint[3]) < 1.e-6f);
        const auto dusk = at(schedule.duskHour);
        assert(dusk.toTimeOfDay() == MumeTimeEnum::DUSK);
        assert(colour(dusk).changing && colour(dusk).tint[3] < night[3]);
        const auto dark = at(schedule.duskHour + 1);
        assert(dark.toTimeOfDay() == MumeTimeEnum::NIGHT);
        assert(!colour(dark).changing);
        assert(std::abs(colour(dark).tint[3] - night[3]) < 1.e-6f);
        // The exact classifier driving the native clock's red/blue warning
        // must match the colour animation for every minute of all 12 months.
        unsigned dawnMinutes = 0, duskMinutes = 0;
        for (int minuteOfDay = 0; minuteOfDay < MUME_MINUTES_PER_DAY; ++minuteOfDay) {
            const MumeMoment moment{2900,
                                    m,
                                    14,
                                    minuteOfDay / MUME_MINUTES_PER_HOUR,
                                    minuteOfDay % MUME_MINUTES_PER_HOUR};
            const auto timeOfDay = moment.toTimeOfDay();
            const bool warning = timeOfDay == MumeTimeEnum::DAWN || timeOfDay == MumeTimeEnum::DUSK;
            assert(colour(moment).changing == warning);
            dawnMinutes += timeOfDay == MumeTimeEnum::DAWN ? 1u : 0u;
            duskMinutes += timeOfDay == MumeTimeEnum::DUSK ? 1u : 0u;
        }
        assert(dawnMinutes == MUME_MINUTES_PER_HOUR && duskMinutes == MUME_MINUTES_PER_HOUR);
    }
    dock->hide();
    assert(!enabled->isChecked() && !seen.back());
    std::cout
        << "Preview step carry/borrow, atomic callbacks, limits, edit semantics, timer and restore passed\n";
}
