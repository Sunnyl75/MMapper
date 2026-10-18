// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 The MMapper Authors
#include "TerrainSpritePacks.h"

#include <array>
#include <stdexcept>

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QPainter>
#include <QRegularExpression>

namespace terrain_sprites {
namespace {
QImage readSheet(const QString &path)
{
    if (!QFile::exists(path))
        return {};
    const QImage sheet{path};
    if (sheet.isNull() || sheet.width() % 3 || sheet.height() % 2
        || sheet.width() / 3 != sheet.height() / 2) {
        qWarning() << "Ignoring invalid organic tree sheet:" << path;
        return {};
    }
    return sheet;
}

QImage tree(const QImage &sheet, const int variant)
{
    const int cell = sheet.width() / 3;
    return sheet.copy((variant % 3) * cell, (variant / 3) * cell, cell, cell)
        .convertToFormat(QImage::Format_ARGB32_Premultiplied)
        .scaled(128, 128, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
}
void appendImages(SpritePacks &result, const std::array<QImage, 24> &trees)
{
    // Two 4x4 atlases per pack: 24 seasonal images, eight empty cells.
    // Even 44 distinct superzones plus the default need only 90 GL layers.
    for (int page = 0; page < 2; ++page) {
        std::vector<QImage> mips;
        for (int size = 128; size >= 1; size /= 2) {
            QImage atlas{size * 4, size * 4, QImage::Format_ARGB32_Premultiplied};
            atlas.fill(Qt::transparent);
            QPainter painter{&atlas};
            for (int tile = 0; tile < 16; ++tile) {
                const int index = page * 16 + tile;
                if (index >= 24)
                    break;
                const auto image = trees[static_cast<std::size_t>(index)]
                                       .scaled(size,
                                               size,
                                               Qt::IgnoreAspectRatio,
                                               Qt::SmoothTransformation);
                painter.drawImage((tile % 4) * size, (3 - tile / 4) * size, image);
            }
            painter.end();
            mips.emplace_back(atlas.mirrored().convertToFormat(QImage::Format_RGBA8888));
        }
        result.atlases.emplace_back(std::move(mips));
    }
}
} // namespace

int appendSeasonalSheet(SpritePacks &result, const QImage &sheet)
{
    if (sheet.isNull() || sheet.width() % 6 || sheet.height() % 4
        || sheet.width() / 6 != sheet.height() / 4) {
        throw std::runtime_error("Invalid bundled seasonal terrain atlas");
    }
    const int base = static_cast<int>(result.atlases.size());
    const int cell = sheet.width() / 6;
    std::array<QImage, 24> images;
    for (int season = 0; season < 4; ++season)
        for (int variant = 0; variant < 6; ++variant) {
            images[static_cast<std::size_t>(season * 6 + variant)]
                = sheet.copy(variant * cell, ((season + 3) % 4) * cell, cell, cell)
                      .convertToFormat(QImage::Format_ARGB32_Premultiplied)
                      .scaled(128, 128, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
        }
    appendImages(result, images);
    return base;
}

SpritePacks loadSpritePacks(const QString &manifest, const QImage &bundled)
{
    if (bundled.isNull() || bundled.width() % 6 || bundled.height() % 4
        || bundled.width() / 6 != bundled.height() / 4) {
        throw std::runtime_error("Invalid bundled seasonal tree atlas");
    }
    SpritePacks result;
    const QDir root{QFileInfo{manifest}.path()};
    const QDir defaults{root.filePath("default")};
    constexpr std::array<const char *, 4> seasons{"winter", "spring", "summer", "autumn"};
    std::array<QImage, 4> global;
    const QImage globalAll = readSheet(defaults.filePath("all.png"));
    for (int s = 0; s < 4; ++s) {
        global[static_cast<std::size_t>(s)] = readSheet(
            defaults.filePath(QString::fromLatin1(seasons[static_cast<std::size_t>(s)]) + ".png"));
    }
    const auto appendPack = [&](const QString &directory) {
        const QDir dir{root.filePath(directory)};
        const QImage all = directory.isEmpty() ? QImage{} : readSheet(dir.filePath("all.png"));
        std::array<QImage, 24> trees;
        for (int s = 0; s < 4; ++s) {
            const auto si = static_cast<std::size_t>(s);
            const QImage local = directory.isEmpty()
                                     ? QImage{}
                                     : readSheet(
                                           dir.filePath(QString::fromLatin1(seasons[si]) + ".png"));
            const QImage &sheet = !local.isNull()        ? local
                                  : !all.isNull()        ? all
                                  : !global[si].isNull() ? global[si]
                                                         : globalAll;
            for (int v = 0; v < 6; ++v) {
                auto &image = trees[static_cast<std::size_t>(s * 6 + v)];
                if (!sheet.isNull())
                    image = tree(sheet, v);
                else {
                    const int cell = bundled.width() / 6;
                    image = bundled.copy(v * cell, ((s + 3) % 4) * cell, cell, cell)
                                .convertToFormat(QImage::Format_ARGB32_Premultiplied)
                                .scaled(128, 128, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
                }
            }
        }
        appendImages(result, trees);
    };
    appendPack({});
    QFile file{manifest};
    if (!file.open(QIODevice::ReadOnly))
        return result;
    QJsonParseError error;
    const auto document = QJsonDocument::fromJson(file.readAll(), &error);
    if (error.error != QJsonParseError::NoError || !document.isObject()) {
        qWarning() << "Ignoring invalid organic terrain manifest:" << manifest;
        return result;
    }
    std::map<QString, int> loaded;
    const auto zones = document.object().value("superzones").toObject();
    const QRegularExpression safeName{QStringLiteral("^[A-Za-z0-9_-]+$")};
    for (auto it = zones.begin(); it != zones.end(); ++it) {
        if (result.areas.size() >= 44) {
            qWarning() << "Organic terrain supports at most 44 superzone overrides";
            break;
        }
        const QString pack = it.value().toString();
        if (it.key().isEmpty() || !safeName.match(pack).hasMatch()) {
            qWarning() << "Ignoring invalid organic terrain pack:" << it.key();
            continue;
        }
        auto [found, inserted] = loaded.emplace(pack, static_cast<int>(result.atlases.size()));
        if (inserted) {
            if (QDir{root.filePath(pack)}.exists())
                appendPack(pack);
            else
                found->second = 0;
        }
        result.areas.emplace(it.key().toUtf8().toStdString(), found->second);
    }
    return result;
}
} // namespace terrain_sprites
