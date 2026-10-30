#pragma once
// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 The MMapper Authors

#include "../global/macros.h"

#include <QWidget>

class QCheckBox;
class QComboBox;
class QLabel;
class QLineEdit;
class QSlider;

class NODISCARD_QOBJECT BackgroundMapPage final : public QWidget
{
    Q_OBJECT

private:
    QCheckBox *m_enabled = nullptr;
    QLineEdit *m_file = nullptr;
    QComboBox *m_tileset = nullptr;
    QLineEdit *m_directory = nullptr;
    QSlider *m_opacity = nullptr;
    QLabel *m_percent = nullptr;
    QCheckBox *m_soften = nullptr;
    QCheckBox *m_shadow = nullptr;

public:
    explicit BackgroundMapPage(QWidget *parent);

public slots:
    void slot_loadConfig();

private:
    void browseMap();
    void browseTiles();
    void selectMap(const QString &filename);
};
