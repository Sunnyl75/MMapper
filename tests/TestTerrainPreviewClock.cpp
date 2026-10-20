// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 The MMapper Authors
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
        assert(m.year == y && m.month == mo - 1 && m.day == d - 1 && m.hour == h - 1
               && m.minute == mi);
    };
    set(2900, 12, 30, 24, 59);
    const auto before = seen.size();
    minute->stepUp();
    expect(2901, 1, 1, 1, 0);
    assert(seen.size() == before + 1);
    minute->stepDown();
    expect(2900, 12, 30, 24, 59);
    set(2900, 5, 12, 24, 10);
    hour->stepUp();
    expect(2900, 5, 13, 1, 10);
    hour->stepDown();
    expect(2900, 5, 12, 24, 10);
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
    set(MUME_START_YEAR, 1, 1, 1, 0);
    minute->stepDown();
    expect(MUME_START_YEAR, 1, 1, 1, 0);
    set(4099, 12, 30, 24, 59);
    minute->stepUp();
    expect(4099, 12, 30, 24, 59);
    set(2900, 5, 12, 24, 10);
    hour->setValue(1);
    expect(2900, 5, 12, 1, 10); // typing/setValue is an edit, not a wrap
    set(2900, 12, 30, 24, 22);
    auto *speed = dock->findChild<QComboBox *>("TerrainPreviewClockSpeed");
    assert(speed);
    speed->setCurrentIndex(speed->findData(60));
    assert(QMetaObject::invokeMethod(timer, "timeout", Qt::DirectConnection));
    expect(2901, 1, 1, 1, 22);
    dock->hide();
    assert(!enabled->isChecked() && !seen.back());
    std::cout
        << "Preview step carry/borrow, atomic callbacks, limits, edit semantics, timer and restore passed\n";
}
