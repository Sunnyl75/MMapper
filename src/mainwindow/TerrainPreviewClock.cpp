// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 The MMapper Authors
#include "TerrainPreviewClock.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <utility>

#include <QCheckBox>
#include <QComboBox>
#include <QDockWidget>
#include <QFormLayout>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QTimer>
#include <QWidget>

namespace {
class StepSpinBox final : public QSpinBox
{
    std::function<void(int)> m_step;

public:
    explicit StepSpinBox(QWidget *parent)
        : QSpinBox{parent}
    {
        setWrapping(true);
    }
    void setStepper(std::function<void(int)> step) { m_step = std::move(step); }

protected:
    void stepBy(const int steps) override
    {
        if (m_step)
            m_step(steps);
    }
};
} // namespace

QDockWidget *createTerrainPreviewClock(QWidget *parent,
                                       std::function<void(std::optional<MumeMoment>)> onChanged)
{
    auto *const dock = new QDockWidget(QObject::tr("Terrain Preview Clock"), parent);
    dock->setObjectName("TerrainPreviewClockDock");
    dock->setAllowedAreas(Qt::TopDockWidgetArea | Qt::BottomDockWidgetArea);
    dock->setFeatures(QDockWidget::DockWidgetMovable | QDockWidget::DockWidgetFloatable
                      | QDockWidget::DockWidgetClosable);
    auto *const widget = new QWidget(dock);
    auto *const layout = new QFormLayout(widget);
    auto *const enabled = new QCheckBox(QObject::tr("Use preview time"), widget);
    enabled->setObjectName("TerrainPreviewClockEnabled");
    enabled->setToolTip(QObject::tr("Preview changes the map's lighting, shadows and seasons. "
                                    "The main MMapper clock continues to show live game time."));
    auto *const year = new QSpinBox(widget);
    auto *const month = new StepSpinBox(widget);
    auto *const day = new StepSpinBox(widget);
    auto *const hour = new StepSpinBox(widget);
    auto *const minute = new StepSpinBox(widget);
    auto *const speed = new QComboBox(widget);
    const std::array<QSpinBox *, 5> controls{year, month, day, hour, minute};
    const std::array<const char *, 5> names{"Year", "Month", "Day", "Hour", "Minute"};
    for (std::size_t i = 0; i < controls.size(); ++i)
        controls[i]->setObjectName(QStringLiteral("TerrainPreviewClock")
                                   + QString::fromLatin1(names[i]));
    speed->setObjectName("TerrainPreviewClockSpeed");
    year->setRange(MUME_START_YEAR, 4099);
    year->setValue(MUME_START_YEAR);
    month->setRange(1, MUME_MONTHS_PER_YEAR);
    month->setValue(10);
    day->setRange(1, MUME_DAYS_PER_MONTH);
    day->setValue(15);
    hour->setRange(0, MUME_HOURS_PER_DAY - 1);
    hour->setValue(12);
    minute->setRange(0, MUME_MINUTES_PER_HOUR - 1);
    speed->addItem(QObject::tr("Paused"), 0);
    speed->addItem(QObject::tr("1 game minute / second"), 1);
    speed->addItem(QObject::tr("10 game minutes / second"), 10);
    speed->addItem(QObject::tr("1 game hour / second"), 60);
    layout->addRow(enabled);
    layout->addRow(QObject::tr("Year"), year);
    layout->addRow(QObject::tr("Month"), month);
    layout->addRow(QObject::tr("Day"), day);
    layout->addRow(QObject::tr("Hour (00–23)"), hour);
    layout->addRow(QObject::tr("Minute"), minute);
    layout->addRow(QObject::tr("Advance"), speed);
    dock->setWidget(widget);

    const auto current = [=]() {
        return MumeMoment{year->value(),
                          month->value() - 1,
                          day->value() - 1,
                          hour->value(),
                          minute->value()};
    };
    const auto apply = [=]() {
        onChanged(enabled->isChecked() ? std::optional{current()} : std::nullopt);
    };
    const auto advance = [=](const int64_t minutes) {
        const auto lower = static_cast<int64_t>(MumeMoment{MUME_START_YEAR, 0, 0, 0, 0}.toSeconds());
        const auto upper = static_cast<int64_t>(MumeMoment{4099, 11, 29, 23, 59}.toSeconds());
        const auto next = MumeMoment::sinceMumeEpoch(
            std::clamp(static_cast<int64_t>(current().toSeconds()) + minutes, lower, upper));
        // One atomic update/callback, never intermediate invalid calendar dates.
        const std::array<QSignalBlocker, 5> blockers{QSignalBlocker{year},
                                                     QSignalBlocker{month},
                                                     QSignalBlocker{day},
                                                     QSignalBlocker{hour},
                                                     QSignalBlocker{minute}};
        year->setValue(next.year);
        month->setValue(next.month + 1);
        day->setValue(next.day + 1);
        hour->setValue(next.hour);
        minute->setValue(next.minute);
        apply();
    };
    month->setStepper(
        [=](int steps) { advance(static_cast<int64_t>(steps) * MUME_MINUTES_PER_MONTH); });
    day->setStepper([=](int steps) { advance(static_cast<int64_t>(steps) * MUME_MINUTES_PER_DAY); });
    hour->setStepper(
        [=](int steps) { advance(static_cast<int64_t>(steps) * MUME_MINUTES_PER_HOUR); });
    minute->setStepper([=](int steps) { advance(steps); });
    QObject::connect(enabled, &QCheckBox::toggled, dock, [=](bool) { apply(); });
    for (auto *control : controls)
        QObject::connect(control, qOverload<int>(&QSpinBox::valueChanged), dock, [=](int) {
            apply();
        });
    auto *const timer = new QTimer(dock);
    timer->setObjectName("TerrainPreviewClockTimer");
    timer->setInterval(1000);
    QObject::connect(timer, &QTimer::timeout, dock, [=]() {
        if (enabled->isChecked() && speed->currentData().toInt() != 0)
            advance(speed->currentData().toInt());
    });
    timer->start();
    QObject::connect(dock, &QDockWidget::visibilityChanged, dock, [=](bool visible) {
        if (!visible)
            enabled->setChecked(false);
    });
    return dock;
}
