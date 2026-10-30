// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 The MMapper Authors

#include "FogOfWarPage.h"

#include "../configuration/configuration.h"

#include <QCheckBox>
#include <QLabel>
#include <QSignalBlocker>
#include <QVBoxLayout>

FogOfWarPage::FogOfWarPage(QWidget *const parent)
    : QWidget(parent)
{
    auto *layout = new QVBoxLayout(this);
    m_enabled = new QCheckBox(tr("Show only rooms known by the current character"), this);
    m_enabled->setObjectName(QStringLiteral("fogOfWarEnabled"));
    layout->addWidget(m_enabled);
    auto *help = new QLabel(
        tr("Unknown areas show the background terrain. Your first history load may take about "
           "a minute; later sessions can use your saved character history while it refreshes. "
           "Rooms without a server ID stay hidden. Free-standing notes and mapping-status "
           "highlights are hidden while fog of war is enabled."),
        this);
    help->setWordWrap(true);
    layout->addWidget(help);
    layout->addStretch();
    connect(m_enabled, &QCheckBox::toggled, this, [](const bool enabled) {
        setConfig().canvas.fogOfWarEnabled.set(enabled);
    });
    slot_loadConfig();
}

void FogOfWarPage::slot_loadConfig()
{
    const QSignalBlocker blocker(m_enabled);
    m_enabled->setChecked(getConfig().canvas.fogOfWarEnabled.get());
}
