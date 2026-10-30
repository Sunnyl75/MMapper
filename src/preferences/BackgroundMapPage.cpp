// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 The MMapper Authors

#include "BackgroundMapPage.h"

#include "../configuration/configuration.h"
#include "../global/window_utils.h"

#include <algorithm>

#include <QCheckBox>
#include <QComboBox>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPointer>
#include <QPushButton>
#include <QSaveFile>
#include <QSignalBlocker>
#include <QSlider>
#include <QStandardPaths>

BackgroundMapPage::BackgroundMapPage(QWidget *parent)
    : QWidget(parent)
{
    auto *layout = new QFormLayout(this);
    layout->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
    layout->setRowWrapPolicy(QFormLayout::WrapLongRows);
    auto *explanation = new QLabel(tr("A separate terrain map beneath the playable map. "
                                      "The background is read-only here; open it as a normal map "
                                      "in another MMapper window to edit it."),
                                   this);
    explanation->setWordWrap(true);
    layout->addRow(explanation);
    m_enabled = new QCheckBox(tr("Show background map"), this);
    m_enabled->setObjectName(QStringLiteral("backgroundMapEnabled"));
    layout->addRow(m_enabled);

    auto *fileRow = new QWidget(this);
    auto *fileLayout = new QHBoxLayout(fileRow);
    fileLayout->setContentsMargins(0, 0, 0, 0);
    m_file = new QLineEdit(fileRow);
    m_file->setObjectName(QStringLiteral("backgroundMapFile"));
    m_file->setPlaceholderText(tr("Choose a .mm2 or .mm2xml map"));
    auto *browse = new QPushButton(tr("Browse…"), fileRow);
    auto *reload = new QPushButton(tr("Reload"), fileRow);
    reload->setToolTip(tr("Read the background map and tiles again after editing their files."));
    fileLayout->addWidget(m_file, 1);
    fileLayout->addWidget(browse);
    fileLayout->addWidget(reload);
    layout->addRow(tr("Map file:"), fileRow);

    m_tileset = new QComboBox(this);
    m_tileset->setObjectName(QStringLiteral("backgroundMapTileset"));
    m_tileset->addItem(tr("Built-in tiles"), QStringLiteral("builtin"));
    m_tileset->addItem(tr("Current game tile pack"), QStringLiteral("foreground"));
    m_tileset->addItem(tr("Separate custom tile pack"), QStringLiteral("custom"));
    layout->addRow(tr("Background tiles:"), m_tileset);

    auto *directoryRow = new QWidget(this);
    auto *directoryLayout = new QHBoxLayout(directoryRow);
    directoryLayout->setContentsMargins(0, 0, 0, 0);
    m_directory = new QLineEdit(directoryRow);
    m_directory->setObjectName(QStringLiteral("backgroundMapTilesDirectory"));
    m_directory->setPlaceholderText(tr("Tile pack folder, or its pixmaps folder"));
    auto *directoryBrowse = new QPushButton(tr("Browse…"), directoryRow);
    directoryLayout->addWidget(m_directory, 1);
    directoryLayout->addWidget(directoryBrowse);
    layout->addRow(tr("Custom pack:"), directoryRow);
    auto *packHelp = new QLabel(tr("Missing custom tiles use the built-in artwork."), this);
    packHelp->setWordWrap(true);
    layout->addRow(packHelp);
    directoryRow->setEnabled(false);
    connect(m_tileset, &QComboBox::currentIndexChanged, this, [this, directoryRow]() {
        const auto choice = m_tileset->currentData().toString();
        directoryRow->setEnabled(choice == QStringLiteral("custom"));
        setConfig().canvas.backgroundMapTileset.set(choice);
    });

    auto *opacityRow = new QWidget(this);
    auto *opacityLayout = new QHBoxLayout(opacityRow);
    opacityLayout->setContentsMargins(0, 0, 0, 0);
    m_opacity = new QSlider(Qt::Horizontal, opacityRow);
    m_opacity->setObjectName(QStringLiteral("backgroundMapOpacity"));
    m_opacity->setAccessibleName(tr("Background map opacity"));
    m_opacity->setRange(0, 100);
    m_percent = new QLabel(opacityRow);
    m_percent->setMinimumWidth(45);
    opacityLayout->addWidget(m_opacity, 1);
    opacityLayout->addWidget(m_percent);
    layout->addRow(tr("Opacity:"), opacityRow);
    m_soften = new QCheckBox(tr("Soften background tile detail"), this);
    m_soften->setObjectName(QStringLiteral("backgroundMapSoften"));
    layout->addRow(m_soften);
    m_shadow = new QCheckBox(tr("Game map casts a soft shadow on the background"), this);
    m_shadow->setObjectName(QStringLiteral("backgroundMapShadow"));
    layout->addRow(m_shadow);

    connect(m_enabled, &QCheckBox::toggled, this, [](const bool enabled) {
        setConfig().canvas.backgroundMapEnabled.set(enabled);
    });
    connect(m_file, &QLineEdit::editingFinished, this, [this]() { selectMap(m_file->text()); });
    connect(browse, &QPushButton::clicked, this, &BackgroundMapPage::browseMap);
    connect(reload, &QPushButton::clicked, this, []() {
        auto &revision = setConfig().canvas.backgroundMapReload;
        revision.set(revision.get() + 1);
    });
    connect(directoryBrowse, &QPushButton::clicked, this, &BackgroundMapPage::browseTiles);
    connect(m_directory, &QLineEdit::editingFinished, this, [this]() {
        setConfig().canvas.backgroundMapTilesDirectory.set(m_directory->text().trimmed());
    });
    connect(m_opacity, &QSlider::valueChanged, this, [this](const int value) {
        m_percent->setText(tr("%1%").arg(value));
        setConfig().canvas.backgroundMapOpacity.set(value);
    });
    connect(m_soften, &QCheckBox::toggled, this, [](const bool value) {
        setConfig().canvas.backgroundMapSoften.set(value);
    });
    connect(m_shadow, &QCheckBox::toggled, this, [](const bool value) {
        setConfig().canvas.backgroundMapShadow.set(value);
    });
#ifdef Q_OS_WASM
    // Browsers cannot expose arbitrary host directories. A deployed tile pack
    // can still be selected by a path in the browser's virtual filesystem.
    directoryBrowse->hide();
    m_directory->setToolTip(tr("Path to a deployed tile pack in the browser filesystem."));
#endif
    slot_loadConfig();
}

void BackgroundMapPage::slot_loadConfig()
{
    const QSignalBlocker enabled(m_enabled), file(m_file), tiles(m_tileset), directory(m_directory),
        opacity(m_opacity), soften(m_soften), shadow(m_shadow);
    const auto &settings = getConfig().canvas;
    m_enabled->setChecked(settings.backgroundMapEnabled.get());
    m_file->setText(settings.backgroundMapFile.get());
    m_tileset->setCurrentIndex(
        std::max(0, m_tileset->findData(settings.backgroundMapTileset.get())));
    m_directory->setText(settings.backgroundMapTilesDirectory.get());
    m_directory->parentWidget()->setEnabled(m_tileset->currentData().toString() == "custom");
    m_opacity->setValue(std::clamp(settings.backgroundMapOpacity.get(), 0, 100));
    m_percent->setText(tr("%1%").arg(m_opacity->value()));
    m_soften->setChecked(settings.backgroundMapSoften.get());
    m_shadow->setChecked(settings.backgroundMapShadow.get());
}

void BackgroundMapPage::selectMap(const QString &filename)
{
    const auto path = filename.trimmed();
    m_file->setText(path);
    setConfig().canvas.backgroundMapFile.set(path);
}

void BackgroundMapPage::browseMap()
{
    const auto filter = tr("MMapper maps (*.mm2 *.mm2xml *.xml);;All files (*)");
#ifdef Q_OS_WASM
    const QPointer<BackgroundMapPage> self(this);
    QFileDialog::getOpenFileContent(filter, [self](const QString &filename, const QByteArray &data) {
        if (!self || filename.isEmpty()) {
            return;
        }
        const auto root = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)
                          + QStringLiteral("/backgrounds");
        QDir().mkpath(root);
        const auto destination = root + "/" + QFileInfo(filename).fileName();
        QSaveFile file(destination);
        if (!file.open(QIODevice::WriteOnly) || file.write(data) != data.size() || !file.commit()) {
            mmqt::showWarning(self,
                              self->tr("Background map"),
                              self->tr("Could not import the map."));
            return;
        }
        self->selectMap(destination);
        auto &revision = setConfig().canvas.backgroundMapReload;
        revision.set(revision.get() + 1);
    });
#else
    auto *dialog = new QFileDialog(this, tr("Choose background map"), m_file->text(), filter);
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->setFileMode(QFileDialog::ExistingFile);
    connect(dialog, &QFileDialog::fileSelected, this, &BackgroundMapPage::selectMap);
    dialog->open();
#endif
}

void BackgroundMapPage::browseTiles()
{
    auto *dialog = new QFileDialog(this, tr("Choose background tile pack"), m_directory->text());
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->setFileMode(QFileDialog::Directory);
    dialog->setOption(QFileDialog::ShowDirsOnly);
    connect(dialog, &QFileDialog::fileSelected, this, [this](const QString &directory) {
        m_directory->setText(directory);
        setConfig().canvas.backgroundMapTilesDirectory.set(directory);
    });
    dialog->open();
}
