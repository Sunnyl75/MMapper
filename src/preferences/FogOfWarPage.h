#pragma once
// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 The MMapper Authors

#include "../global/macros.h"

#include <QWidget>

class QCheckBox;

class NODISCARD_QOBJECT FogOfWarPage final : public QWidget
{
    Q_OBJECT

private:
    QCheckBox *m_enabled = nullptr;

public:
    explicit FogOfWarPage(QWidget *parent);

public slots:
    void slot_loadConfig();
};
