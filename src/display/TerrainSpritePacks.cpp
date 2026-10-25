// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 The MMapper Authors
#include "TerrainSpritePacks.h"

#include "../global/Charset.h"
#include "TerrainMaterialMaps.h"

#include <algorithm>
#include <array>
#include <cstdint>
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

// Fit authored rounded shore silhouettes into a fixed padded atlas frame.
// The shader restores their long, shallow physical proportions. Empty or
// invalid sheets stay absent instead of producing a rectangular fallback.
QImage alignRoadShoreSheet(const QImage &input)
{
    if (input.isNull() || input.width() % 6 || input.height() % 4
        || input.width() / 6 != input.height() / 4)
        return {};
    const auto source = input.convertToFormat(QImage::Format_ARGB32);
    const int sourceCell = source.width() / 6;
    constexpr int cell = 256;
    QImage result{6 * cell, 4 * cell, QImage::Format_ARGB32_Premultiplied};
    result.fill(Qt::transparent);
    QPainter painter{&result};
    painter.setRenderHint(QPainter::SmoothPixmapTransform);
    for (int slot = 0; slot < 24; ++slot) {
        const int col = slot % 6, row = slot / 6;
        QRect bounds;
        for (int y = row * sourceCell; y < (row + 1) * sourceCell; ++y)
            for (int x = col * sourceCell; x < (col + 1) * sourceCell; ++x)
                if (qAlpha(source.pixel(x, y)) >= 32)
                    bounds |= QRect{x, y, 1, 1};
        if (!bounds.isEmpty())
            painter.drawImage(QRect{col * cell + 2, row * cell + 2, cell - 4, cell - 4},
                              source,
                              bounds);
    }
    return result;
}

// Both states use the SAME authored posts and two-room frame. The old source
// open row has a swung leaf: instead, expose the closed row's posts alone.
QImage alignDoorSheet(const QImage &input)
{
    if (input.isNull() || input.width() % 6 || input.height() % 2
        || input.width() / 6 != input.height() / 2)
        return {};
    const auto source = input.convertToFormat(QImage::Format_ARGB32);
    const int sourceCell = source.width() / 6;
    std::array<std::pair<int, int>, 2> bands{};
    unsigned band = 0;
    int first = -1, last = -1;
    for (int y = 0; y < source.height(); ++y) {
        int count = 0;
        for (int x = 0; x < source.width(); ++x)
            count += qAlpha(source.pixel(x, y)) >= 64;
        if (count < 4)
            continue;
        if (first < 0)
            first = y;
        else if (y - last > 8) {
            if (band >= bands.size())
                return {};
            bands[band++] = {first, last + 1};
            first = y;
        }
        last = y;
    }
    if (first < 0 || band != 1)
        return {};
    bands[band] = {first, last + 1};
    constexpr int cell = 256;
    QImage aligned{cell * 6, cell * 4, QImage::Format_ARGB32_Premultiplied};
    aligned.fill(Qt::transparent);
    QPainter painter{&aligned};
    painter.setRenderHint(QPainter::SmoothPixmapTransform);
    const double scale = cell * 0.40 / sourceCell;
    const double halfJamb = (bands[0].second - bands[0].first) * 0.5;
    for (int row = 0; row < 4; ++row) {
        const auto limits = bands[0];
        const double hinge = limits.first + halfJamb;
        for (int col = 0; col < 6; ++col) {
            // Above the leaf, the alpha silhouette contains two separate post
            // caps. Their widest internal gap locates this variant's doorway.
            int gapStart = -1, bestStart = 0, bestEnd = 0;
            bool seenPost = false;
            for (int x = 0; x < sourceCell; ++x) {
                bool cap = false;
                for (int y = limits.first + (limits.second - limits.first) / 8;
                     y < limits.first + (limits.second - limits.first) / 5;
                     ++y)
                    cap |= qAlpha(source.pixel(col * sourceCell + x, y)) >= 64;
                if (cap) {
                    if (gapStart >= 0 && x - gapStart > bestEnd - bestStart) {
                        bestStart = gapStart;
                        bestEnd = x;
                    }
                    seenPost = true;
                    gapStart = -1;
                } else if (seenPost && gapStart < 0) {
                    gapStart = x;
                }
            }
            if (row % 2 != 0 && bestEnd - bestStart < sourceCell / 4)
                continue; // No appropriate posts: do not invent an open sprite.
            painter.save();
            painter.setClipRect(QRect{col * cell, row * cell, cell, cell});
            painter.drawImage(QRectF{col * cell + cell * 0.30,
                                     row * cell + cell * 0.5 + (limits.first - hinge) * scale,
                                     sourceCell * scale,
                                     (limits.second - limits.first) * scale},
                              source,
                              QRect{col * sourceCell,
                                    limits.first,
                                    sourceCell,
                                    limits.second - limits.first});
            if (row % 2 != 0) {
                painter.setCompositionMode(QPainter::CompositionMode_Clear);
                painter.fillRect(QRectF{col * cell + cell * 0.30 + bestStart * scale,
                                        static_cast<double>(row * cell),
                                        (bestEnd - bestStart) * scale,
                                        static_cast<double>(cell)},
                                 Qt::transparent);
            }
            painter.restore();
        }
    }
    return aligned;
}

// Normalize authored fixed-role sheets to the existing 24-cell atlas ABI.
// Repeat source roles, never synthesize seasons or alter the source PNG.
QImage expandFixedSheet(const QImage &source, const int columns, const int rows)
{
    if (source.isNull() || columns <= 0 || rows <= 0 || source.width() % columns
        || source.height() % rows || source.width() / columns != source.height() / rows)
        return {};
    constexpr int cell = 256;
    const int sourceCell = source.width() / columns;
    QImage expanded{6 * cell, 4 * cell, QImage::Format_ARGB32_Premultiplied};
    expanded.fill(Qt::transparent);
    QPainter painter{&expanded};
    painter.setRenderHint(QPainter::SmoothPixmapTransform);
    for (int slot = 0; slot < 24; ++slot) {
        const int role = slot % (columns * rows);
        painter.drawImage(QRect{(slot % 6) * cell, (slot / 6) * cell, cell, cell},
                          source,
                          QRect{(role % columns) * sourceCell,
                                (role / columns) * sourceCell,
                                sourceCell,
                                sourceCell});
    }
    return expanded;
}

QImage alignSeasonalSheet(const QImage &input, const terrain_materials::Placement placement)
{
    using P = terrain_materials::Placement;
    // Soft water patches are authored on an exact grid. Cropping them to an
    // alpha-64 bounding box throws away the faint wash and creates square cuts.
    if ((placement == P::MATERIAL || placement == P::REPEAT || placement == P::SOFT_PATCH)
        || input.isNull() || input.width() % 6 || input.height() % 4
        || input.width() / 6 != input.height() / 4)
        return input;
    const auto source = input.convertToFormat(QImage::Format_ARGB32);
    const int cell = source.width() / 6;
    // Generated atlases can have four clearly separated rows without placing
    // them on exact quarter-height boundaries. Detect alpha gutters before
    // slicing so a hill or branch is never cut into the following season.
    std::vector<std::pair<int, int>> rows;
    int begin = -1;
    int last = -1;
    for (int y = 0; y < source.height(); ++y) {
        int count = 0;
        for (int x = 0; x < source.width(); ++x)
            count += qAlpha(source.pixel(x, y)) >= 64;
        if (count < 4)
            continue;
        if (begin < 0)
            begin = y;
        else if (y - last > 8) {
            rows.emplace_back(begin, last + 1);
            begin = y;
        }
        last = y;
    }
    if (begin >= 0)
        rows.emplace_back(begin, last + 1);
    if (rows.size() != 4)
        return (placement == P::WALL || placement == P::EAVE || placement == P::MASONRY
                || placement == P::FINISH)
                   ? QImage{}
                   : input; // Exact-grid authored overrides need no automatic repair.
    std::array<QRect, 24> bounds;
    int maxWidth = 1;
    int maxHeight = 1;
    for (int row = 0; row < 4; ++row) {
        std::vector<std::pair<int, int>> columns;
        if (placement == P::EAVE) {
            // Generated modules may straddle nominal cell boundaries. Find
            // the six separated pieces within each role row, not fragments
            // of a neighbouring strip caught by a fixed-grid crop.
            int first = -1;
            int previous = -1;
            for (int x = 0; x < source.width(); ++x) {
                int count = 0;
                for (int y = rows[static_cast<size_t>(row)].first;
                     y < rows[static_cast<size_t>(row)].second;
                     ++y)
                    count += qAlpha(source.pixel(x, y)) >= 64;
                if (count < 4)
                    continue;
                if (first < 0)
                    first = x;
                else if (x - previous > 4) {
                    columns.emplace_back(first, previous + 1);
                    first = x;
                }
                previous = x;
            }
            if (first >= 0)
                columns.emplace_back(first, previous + 1);
            if (columns.size() != 6)
                return {}; // Malformed roof kits have no unrelated fallback.
        }
        for (int col = 0; col < 6; ++col) {
            const int left = columns.empty() ? col * cell : columns[static_cast<size_t>(col)].first;
            const int right = columns.empty() ? (col + 1) * cell
                                              : columns[static_cast<size_t>(col)].second;
            QRect used;
            for (int y = rows[static_cast<std::size_t>(row)].first;
                 y < rows[static_cast<std::size_t>(row)].second;
                 ++y)
                for (int x = left; x < right; ++x)
                    if (qAlpha(source.pixel(x, y)) >= 64)
                        used |= QRect{x, y, 1, 1};
            if (!used.isEmpty()) {
                used = used.adjusted(-2, -2, 2, 2)
                           .intersected(columns.empty()
                                            ? QRect{col * cell, 0, cell, source.height()}
                                            : source.rect());
                maxWidth = std::max(maxWidth, used.width());
                maxHeight = std::max(maxHeight, used.height());
            }
            bounds[static_cast<std::size_t>(row * 6 + col)] = used;
        }
    }
    if (placement == P::FINISH || placement == P::EAVE) {
        QImage result{source.size(), QImage::Format_ARGB32_Premultiplied};
        result.fill(Qt::transparent);
        QPainter caps{&result};
        caps.setRenderHint(QPainter::SmoothPixmapTransform);
        for (int row = 0; row < 4; ++row)
            for (int col = 0; col < 6; ++col) {
                auto from = bounds[static_cast<size_t>(row * 6 + col)].adjusted(2, 2, -2, -2);
                if (from.isEmpty())
                    continue;
                if (placement == P::EAVE && row == 0) {
                    // One common room-length frame, with two courses. Match
                    // opposing ends before making mips, so a run can continue
                    // across rooms without a bounding-box outline or gap.
                    QImage strip = source.copy(from).scaled(cell,
                                                            qRound(cell * 0.24),
                                                            Qt::IgnoreAspectRatio,
                                                            Qt::SmoothTransformation);
                    for (int y = 0; y < strip.height(); ++y) {
                        const auto a = strip.pixelColor(0, y);
                        const auto b = strip.pixelColor(cell - 1, y);
                        const QColor edge{(a.red() + b.red()) / 2,
                                          (a.green() + b.green()) / 2,
                                          (a.blue() + b.blue()) / 2,
                                          (a.alpha() + b.alpha()) / 2};
                        strip.setPixelColor(0, y, edge);
                        strip.setPixelColor(cell - 1, y, edge);
                    }
                    caps.drawImage(col * cell, 0, strip);
                    continue;
                }
                if ((placement == P::FINISH && row != 2) || (placement == P::EAVE && row != 3)) {
                    // Square corner/pier art keeps its authored proportions.
                    const int side = std::max(from.width(), from.height());
                    const auto center = from.center();
                    from = QRect{center.x() - side / 2, center.y() - side / 2, side, side};
                }
                // Narrow boards are normalized only for storage; the shader
                // restores their physical width. Roof roles share one sheet.
                caps.drawImage(QRect{col * cell, row * cell, cell, cell}, source, from);
            }
        caps.end();
        return result;
    }
    const double widthFraction = placement == P::UPRIGHT ? 0.86 : 0.96;
    const double height = placement == P::UPRIGHT ? 0.84 : placement == P::BORDER ? 0.52 : 0.96;
    const double scale = std::min(cell * widthFraction / maxWidth, cell * height / maxHeight);
    QImage aligned{source.size(), QImage::Format_ARGB32_Premultiplied};
    aligned.fill(Qt::transparent);
    QPainter painter{&aligned};
    painter.setRenderHint(QPainter::SmoothPixmapTransform);
    for (int row = 0; row < 4; ++row) {
        for (int col = 0; col < 6; ++col) {
            const auto from = bounds[static_cast<std::size_t>(row * 6 + col)];
            if (from.isEmpty())
                continue;
            const QSizeF size{from.width() * scale, from.height() * scale};
            const double y = placement == P::UPRIGHT ? cell * 0.94 - size.height()
                                                     : (cell - size.height()) * 0.5;
            painter.drawImage(QRectF{col * cell + (cell - size.width()) * 0.5,
                                     row * cell + y,
                                     size.width(),
                                     size.height()},
                              source,
                              from);
        }
    }
    painter.end();
    if (placement == P::WALL || placement == P::MASONRY) {
        // Edge sprites use a strict room-local frame. Crop alpha gutters, fit
        // the strip to its boundary, and extrude opaque edge texels outward so
        // the underlying line cannot show through a transparent outer margin.
        aligned.fill(Qt::transparent);
        QPainter wallPainter{&aligned};
        wallPainter.setRenderHint(QPainter::SmoothPixmapTransform);
        const int depth = std::max(1, static_cast<int>(cell * 0.20));
        for (int row = 0; row < 4; ++row)
            for (int col = 0; col < 6; ++col) {
                auto from = bounds[static_cast<std::size_t>(row * 6 + col)];
                if (from.isEmpty())
                    continue;
                from = from.adjusted(2, 2, -2, -2);
                if (from.isEmpty())
                    continue;
                if (placement == P::MASONRY) {
                    // Repeat at the physical wall depth, preserving the source
                    // stone aspect ratio instead of elongating a short strip.
                    const double width = cell
                                         * static_cast<double>(terrain_materials::STONE_WALL_DEPTH)
                                         * from.width() / from.height();
                    wallPainter.save();
                    wallPainter.setClipRect(QRect{col * cell, row * cell, cell, depth});
                    for (double x = 0.0; x < cell; x += width)
                        wallPainter.drawImage(QRectF{col * cell + x,
                                                     static_cast<double>(row * cell),
                                                     width,
                                                     static_cast<double>(depth)},
                                              source,
                                              from);
                    wallPainter.restore();
                } else {
                    wallPainter.drawImage(QRect{col * cell, row * cell, cell, depth}, source, from);
                }
            }
        wallPainter.end();
        for (int row = 0; row < 4; ++row)
            for (int col = 0; col < 6; ++col) {
                const int x0 = col * cell;
                const int y0 = row * cell;
                std::vector<QRgb> edge(static_cast<std::size_t>(cell), 0);
                for (int x = 0; x < cell; ++x)
                    for (int y = 0; y < depth; ++y) {
                        const auto pixel = aligned.pixel(x0 + x, y0 + y);
                        if (qAlpha(pixel) >= 128) {
                            edge[static_cast<std::size_t>(x)]
                                = qRgb(qRed(pixel) * 255 / qAlpha(pixel),
                                       qGreen(pixel) * 255 / qAlpha(pixel),
                                       qBlue(pixel) * 255 / qAlpha(pixel));
                            break;
                        }
                    }
                // Empty cells remain transparent: missing artwork never gets
                // replaced by a procedural rectangle or an unrelated material.
                for (int x = 0; x < cell; ++x) {
                    auto color = edge[static_cast<std::size_t>(x)];
                    for (int distance = 1; color == 0 && distance < cell; ++distance) {
                        if (x + distance < cell)
                            color = edge[static_cast<std::size_t>(x + distance)];
                        if (color == 0 && x - distance >= 0)
                            color = edge[static_cast<std::size_t>(x - distance)];
                    }
                    if (color == 0)
                        continue;
                    for (int y = 0; y < std::max(1, cell / 64); ++y)
                        aligned.setPixel(x0 + x, y0 + y, color);
                }
            }
    }
    return aligned;
}

void applyRoofCaps(QImage &finishes, const QImage &caps)
{
    if (finishes.isNull())
        return;
    const int cell = finishes.width() / 6;
    QPainter painter{&finishes};
    painter.setCompositionMode(QPainter::CompositionMode_Source);
    // These formerly held square roof patches. Optional decorative caps
    // must never fall back to those unrelated pieces when their art is absent.
    painter.fillRect(0, 0, finishes.width(), cell * 2, Qt::transparent);
    if (caps.isNull() || caps.width() % 3 || caps.height() % 2
        || caps.width() / 3 != caps.height() / 2)
        return;
    painter.setRenderHint(QPainter::SmoothPixmapTransform);
    const auto source = caps.convertToFormat(QImage::Format_ARGB32);
    const int size = source.width() / 3;
    for (int row = 0; row < 2; ++row)
        for (int col = 0; col < 3; ++col) {
            QRect used;
            for (int y = row * size; y < (row + 1) * size; ++y)
                for (int x = col * size; x < (col + 1) * size; ++x)
                    if (qAlpha(source.pixel(x, y)) >= 64)
                        used |= QRect{x, y, 1, 1};
            if (used.isEmpty())
                continue;
            const int side = std::max(used.width(), used.height()) + 4;
            // The top/left shoulders define the wall-corner origin. Centring
            // a shorter rounded silhouette would move those joins inward.
            const QRect frame{used.x() - 2, used.y() - 2, side, side};
            // Uniform square framing, duplicated to preserve six variants
            // per role in the existing two-layer finishing atlas.
            for (int variant = col; variant < 6; variant += 3)
                painter.drawImage(QRect{variant * cell, row * cell, cell, cell}, source, frame);
        }
}

// Repeat materials occupy complete texture pages, so no sprite-cell joins can
// appear. Opposite edges match at every mip level, including minified views.
int appendRepeatMaterial(SpritePacks &result, const QImage &source)
{
    if (source.isNull() || source.width() != source.height())
        throw std::runtime_error("Invalid repeating terrain material");
    const int base = static_cast<int>(result.atlases.size());
    auto image = source.scaled(512, 512, Qt::IgnoreAspectRatio, Qt::SmoothTransformation)
                     .convertToFormat(QImage::Format_ARGB32);
    const auto blend = [](const QRgb a, const QRgb b, const double weight) {
        return qRgb(static_cast<int>(qRed(a) * (1.0 - weight) + qRed(b) * weight),
                    static_cast<int>(qGreen(a) * (1.0 - weight) + qGreen(b) * weight),
                    static_cast<int>(qBlue(a) * (1.0 - weight) + qBlue(b) * weight));
    };
    for (int axis = 0; axis < 2; ++axis)
        for (int k = 0; k < 20; ++k) {
            const double t = k / 20.0;
            const double weight = 0.5 * (1.0 - t * t * (3.0 - 2.0 * t));
            for (int along = 0; along < 512; ++along) {
                const QPoint a = axis == 0 ? QPoint{k, along} : QPoint{along, k};
                const QPoint b = axis == 0 ? QPoint{511 - k, along} : QPoint{along, 511 - k};
                const auto pa = image.pixel(a), pb = image.pixel(b);
                image.setPixel(a, blend(pa, pb, weight));
                image.setPixel(b, blend(pb, pa, weight));
            }
        }
    std::vector<QImage> mips;
    for (int size = 512; size >= 4; size /= 2) {
        auto mip = image.scaled(size, size, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
        for (int y = 0; y < size; ++y) {
            const auto color = blend(mip.pixel(0, y), mip.pixel(size - 1, y), 0.5);
            mip.setPixel(0, y, color);
            mip.setPixel(size - 1, y, color);
        }
        for (int x = 0; x < size; ++x) {
            const auto color = blend(mip.pixel(x, 0), mip.pixel(x, size - 1), 0.5);
            mip.setPixel(x, 0, color);
            mip.setPixel(x, size - 1, color);
        }
        mips.push_back(mip.mirrored().convertToFormat(QImage::Format_RGBA8888));
    }
    result.atlases.push_back(mips);
    result.atlases.push_back(std::move(mips));
    return base;
}

namespace {
int appendSheet(SpritePacks &result, const QImage &sheet, const bool seasonal)
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
                = sheet
                      .copy(variant * cell,
                            (seasonal ? (season + 3) % 4 : season) * cell,
                            cell,
                            cell)
                      .convertToFormat(QImage::Format_ARGB32_Premultiplied)
                      .scaled(128, 128, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
        }
    appendImages(result, images);
    return base;
}

} // namespace
int appendSeasonalSheet(SpritePacks &result, const QImage &sheet)
{
    return appendSheet(result, sheet, true);
}
int appendFixedSheet(SpritePacks &result, const QImage &sheet)
{
    return appendSheet(result, sheet, false);
}

SpritePacks loadSpritePacks(const QString &manifest,
                            const QImage &bundled,
                            const QStringList &namedRoots)
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
    const auto appendPack = [&](const QStringList &directories) {
        std::array<QImage, 24> trees;
        for (int s = 0; s < 4; ++s) {
            const auto si = static_cast<std::size_t>(s);
            QImage sheet;
            for (const auto &directory : directories) {
                const QDir dir{directory};
                sheet = readSheet(dir.filePath(QString::fromLatin1(seasons[si]) + ".png"));
                if (sheet.isNull())
                    sheet = readSheet(dir.filePath("all.png"));
                if (!sheet.isNull())
                    break;
            }
            if (sheet.isNull())
                sheet = !global[si].isNull() ? global[si] : globalAll;
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
    const auto readZones = [](const QString &path) {
        QFile file{path};
        if (!file.open(QIODevice::ReadOnly))
            return QJsonObject{};
        QJsonParseError error;
        const auto document = QJsonDocument::fromJson(file.readAll(), &error);
        if (error.error != QJsonParseError::NoError || !document.isObject()) {
            qWarning() << "Ignoring invalid organic terrain manifest:" << path;
            return QJsonObject{};
        }
        return document.object().value("superzones").toObject();
    };
    const QString builtinManifest{QStringLiteral(":/pixmaps/organic-terrain/packs.json")};
    auto zones = readZones(builtinManifest);
    if (manifest != builtinManifest) {
        const auto overrides = readZones(manifest);
        for (auto it = overrides.begin(); it != overrides.end(); ++it)
            zones.insert(it.key(), it.value());
    }
    // Optional aliases remain available for sharing one pack between areas.
    // Named folders need no manifest; user roots override asset-root aliases.
    for (auto it = namedRoots.crbegin(); it != namedRoots.crend(); ++it) {
        const auto overrides = readZones(QDir{*it}.filePath(QStringLiteral("packs.json")));
        for (auto zone = overrides.begin(); zone != overrides.end(); ++zone)
            zones.insert(zone.key(), zone.value());
    }
    QStringList roots = namedRoots;
    roots.append(root.path());
    roots.append(QStringLiteral(":/pixmaps/organic-terrain"));
    roots.removeDuplicates();
    std::map<QString, int> loaded;
    const auto loadPack = [&](const QString &pack) {
        const auto existing = loaded.find(pack);
        if (existing != loaded.end())
            return existing->second;
        QStringList directories;
        for (const auto &base : roots) {
            const auto directory = QDir{base}.filePath(pack);
            if (QDir{directory}.exists())
                directories.append(directory);
        }
        int atlas = 0;
        if (!directories.isEmpty() && result.directories.size() < 44) {
            atlas = static_cast<int>(result.atlases.size());
            appendPack(directories);
            result.directories.emplace(atlas, directories);
        } else if (!directories.isEmpty()) {
            qWarning() << "Organic terrain supports at most 44 distinct packs:" << pack;
        }
        loaded.emplace(pack, atlas);
        return atlas;
    };
    // Direct folder names use the same key as area media. Scan only once on
    // texture load; room drawing resolves the prepared in-memory selection.
    for (const auto &base : namedRoots) {
        const QDir directory{base};
        for (const auto &entry :
             directory.entryList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name)) {
            if (entry == QStringLiteral("default"))
                continue;
            const QDir pack{directory.filePath(entry)};
            if (pack.entryList({QStringLiteral("*.png")}, QDir::Files).isEmpty())
                continue;
            result.namedAreas.try_emplace(entry.toUtf8().toStdString(), loadPack(entry));
        }
    }
    const QRegularExpression safeName{QStringLiteral("^[A-Za-z0-9_-]+$")};
    for (auto it = zones.begin(); it != zones.end(); ++it) {
        const QString pack = it.value().toString();
        if (it.key().isEmpty() || !safeName.match(pack).hasMatch()) {
            qWarning() << "Ignoring invalid organic terrain pack:" << it.key();
            continue;
        }
        result.areas.emplace(it.key().toUtf8().toStdString(), loadPack(pack));
    }
    return result;
}

int PackSelection::atlas(const terrain_materials::Family family) const
{
    return materials.at(static_cast<std::size_t>(family));
}
bool PackSelection::has(const terrain_materials::Family family) const
{
    return available.at(static_cast<std::size_t>(family));
}
int PackSelections::index(const std::string_view area) const
{
    if (!namedAreas.empty()) {
        static const QRegularExpression article{QStringLiteral("^the\\s+")};
        auto name = QString::fromUtf8(area.data(), static_cast<qsizetype>(area.size()))
                        .toLower()
                        .remove(article)
                        .replace(' ', '-');
        mmqt::toAsciiInPlace(name);
        const auto named = namedAreas.find(name.toUtf8().toStdString());
        if (named != namedAreas.end() && named->second != 0)
            return named->second;
    }
    const auto found = areas.find(area);
    return found == areas.end() ? 0 : found->second;
}
const PackSelection &PackSelections::find(const std::string_view area) const
{
    return packs.at(static_cast<std::size_t>(index(area)));
}

namespace {
using F = terrain_materials::Family;
using P = terrain_materials::Placement;
bool fixedFamily(const F family)
{
    return family == F::DOOR || family == F::DEATH_TRAP || family == F::ROAD_SHORE
           || family == F::WATER_FLOW || family == F::RAPIDS || family == F::UNDERWATER_GROUND
           || family == F::UNDERWATER_DETAIL
           || terrain_materials::SHEETS.at(static_cast<std::size_t>(family)).placement == P::FINISH
           || terrain_materials::SHEETS.at(static_cast<std::size_t>(family)).placement == P::EAVE;
}
bool reflectiveFamily(const F family)
{
    return std::any_of(terrain_material_maps::MATERIALS.begin(),
                       terrain_material_maps::MATERIALS.end(),
                       [family](const auto &material) { return material.family == family; });
}
QImage prepareMaterial(QImage sheet, const F family, const QImage &caps, const bool allSeason)
{
    if (sheet.isNull())
        return {};
    const auto placement = terrain_materials::SHEETS.at(static_cast<std::size_t>(family)).placement;
    if (allSeason)
        sheet = expandFixedSheet(sheet, 3, 2);
    else if (family == F::ROAD_SHORE)
        sheet = alignRoadShoreSheet(sheet);
    else if (family == F::DOOR)
        sheet = alignDoorSheet(sheet);
    else if (family == F::DEATH_TRAP || family == F::WATER_FLOW)
        sheet = expandFixedSheet(sheet, 1, 1);
    else if (family == F::UNDERWATER_GROUND || family == F::UNDERWATER_DETAIL)
        sheet = expandFixedSheet(sheet, family == F::UNDERWATER_GROUND ? 3 : 6, 2);
    if (sheet.isNull())
        return {};
    if (placement == P::REPEAT) {
        if (sheet.width() != sheet.height())
            return {};
    } else if (sheet.width() % 6 || sheet.height() % 4 || sheet.width() / 6 != sheet.height() / 4)
        return {};
    sheet = alignSeasonalSheet(sheet, placement);
    if (placement == P::FINISH)
        applyRoofCaps(sheet, caps);
    return sheet;
}
int appendMaterial(SpritePacks &sprites, const QImage &sheet, const F family)
{
    if (terrain_materials::SHEETS.at(static_cast<std::size_t>(family)).placement == P::REPEAT)
        return appendRepeatMaterial(sprites, sheet);
    return fixedFamily(family) ? appendFixedSheet(sprites, sheet)
                               : appendSeasonalSheet(sprites, sheet);
}
QString allSeasonFilename(QString filename)
{
    filename.replace(QStringLiteral("-seasons.png"), QStringLiteral("-all.png"));
    return filename;
}
} // namespace

PackSelections loadMaterialPacks(SpritePacks &sprites,
                                 const ImageLoader &loadGlobal,
                                 const int maxLayers)
{
    // Reserve one metadata page and the eight shared reflection pages. Each
    // reflective override reserves its own data pair before accepting its art.
    constexpr int stride = terrain_materials::LAYERS_PER_SHEET;
    int reserved = 1 + stride * static_cast<int>(terrain_material_maps::MATERIALS.size());
    if (static_cast<int>(sprites.atlases.size())
            + stride * static_cast<int>(terrain_materials::SHEETS.size()) + reserved
        > maxLayers)
        throw std::runtime_error("Insufficient texture array layers for terrain defaults");
    PackSelections result;
    PackSelection defaults;
    const auto globalCaps = loadGlobal(QStringLiteral("terrain-roof-caps.png"));
    QImage globalFinishes;
    constexpr auto &sheets = terrain_materials::SHEETS;
    for (std::size_t i = 0; i < sheets.size(); ++i) {
        const auto family = static_cast<F>(i);
        const auto filename = QString::fromLatin1(terrain_materials::SHEETS[i].filename);
        auto source = loadGlobal(filename);
        auto sheet = prepareMaterial(source, family, globalCaps, false);
        if (sheet.isNull()) {
            qWarning() << "Invalid terrain material; using bundled fallback:" << filename;
            source = QImage{QStringLiteral(":/pixmaps/") + filename};
            sheet = prepareMaterial(source, family, globalCaps, false);
        }
        if (family == F::WALL_FINISH)
            globalFinishes = source;
        defaults.available[i] = !sheet.isNull();
        if (sheet.isNull()) {
            sheet = QImage{1536, 1024, QImage::Format_ARGB32_Premultiplied};
            sheet.fill(Qt::transparent);
        }
        defaults.materials[i] = appendMaterial(sprites, sheet, family);
    }
    result.packs.push_back(defaults);
    std::map<int, int> selections{{0, 0}};
    for (const auto &[treeBase, directories] : sprites.directories) {
        auto selection = defaults;
        selection.trees = treeBase;
        QImage localCaps;
        for (const auto &directory : directories) {
            localCaps = QImage{QDir{directory}.filePath(QStringLiteral("terrain-roof-caps.png"))};
            if (!localCaps.isNull())
                break;
        }
        for (std::size_t i = 0; i < sheets.size(); ++i) {
            const auto family = static_cast<F>(i);
            const auto filename = QString::fromLatin1(terrain_materials::SHEETS[i].filename);
            QImage sheet;
            QString path;
            for (const auto &directory : directories) {
                const QDir dir{directory};
                path = dir.filePath(filename);
                bool allSeason = false;
                if (!QFile::exists(path) && !fixedFamily(family)
                    && filename.endsWith(QStringLiteral("-seasons.png"))) {
                    path = dir.filePath(allSeasonFilename(filename));
                    allSeason = true;
                }
                if (!QFile::exists(path))
                    continue;
                sheet = prepareMaterial(QImage{path},
                                        family,
                                        localCaps.isNull() ? globalCaps : localCaps,
                                        allSeason);
                if (!sheet.isNull())
                    break;
                qWarning() << "Ignoring invalid regional terrain material:" << path;
            }
            if (sheet.isNull() && family == F::WALL_FINISH && !localCaps.isNull())
                sheet = prepareMaterial(globalFinishes, family, localCaps, false);
            if (sheet.isNull())
                continue;
            const int dataLayers = reflectiveFamily(family) ? stride : 0;
            if (static_cast<int>(sprites.atlases.size()) + stride + reserved + dataLayers
                > maxLayers) {
                qWarning() << "Texture layer limit; retaining default terrain material:" << path;
                continue;
            }
            selection.materials[i] = appendMaterial(sprites, sheet, family);
            selection.available[i] = true;
            reserved += dataLayers;
        }
        // These optional detail sheets do not change the stable family ABI.
        // Six fixed roles, on a 3x2 grid; invalid or absent files draw nothing.
        for (const auto &[filename, destination] :
             {std::pair{"terrain-road-cart-tracks.png", &selection.cartTracks},
              std::pair{"terrain-city-wall-fringe.png", &selection.cityWallFringe}}) {
            for (const auto &directory : directories) {
                const auto path = QDir{directory}.filePath(QString::fromLatin1(filename));
                auto source = QImage{path};
                if (destination == &selection.cityWallFringe && !source.isNull()
                    && source.width() % 3 == 0 && source.height() % 2 == 0
                    && source.width() / 3 == source.height() / 2) {
                    // Crop only the common vertical alpha band, preserving
                    // each strip's authored width and endpoint coordinates.
                    const int cell = source.width() / 3;
                    QImage aligned{source.size(), QImage::Format_ARGB32_Premultiplied};
                    aligned.fill(Qt::transparent);
                    QPainter painter{&aligned};
                    for (int role = 0; role < 6; ++role) {
                        const auto tile = source.copy((role % 3) * cell,
                                                      (role / 3) * cell,
                                                      cell,
                                                      cell);
                        int top = cell, bottom = 0;
                        for (int y = 0; y < cell; ++y)
                            for (int x = 0; x < cell; ++x)
                                if (tile.pixelColor(x, y).alpha() >= 16) {
                                    top = std::min(top, y);
                                    bottom = std::max(bottom, y + 1);
                                }
                        if (bottom > top)
                            painter.drawImage(QRect{(role % 3) * cell, (role / 3) * cell, cell, cell},
                                              tile,
                                              QRect{0, top, cell, bottom - top});
                    }
                    painter.end();
                    source = aligned;
                }
                const auto detail = expandFixedSheet(source, 3, 2);
                if (detail.isNull())
                    continue;
                if (static_cast<int>(sprites.atlases.size()) + stride + reserved <= maxLayers)
                    *destination = appendFixedSheet(sprites, detail);
                else
                    qWarning() << "Texture layer limit; omitting optional terrain detail:" << path;
                break;
            }
        }
        selections.emplace(treeBase, static_cast<int>(result.packs.size()));
        result.packs.push_back(selection);
    }
    for (const auto &[area, treeBase] : sprites.areas)
        result.areas.emplace(area, selections.at(treeBase));
    for (const auto &[name, treeBase] : sprites.namedAreas)
        result.namedAreas.emplace(name, selections.at(treeBase));
    return result;
}

std::vector<QImage> packLookup(const SpritePacks &sprites,
                               const PackSelections &selections,
                               const std::vector<std::array<int, 3>> &reflectionSources)
{
    QImage table{512, 512, QImage::Format_RGBA8888};
    table.fill(Qt::transparent);
    for (std::size_t row = 0; row < selections.packs.size(); ++row) {
        const auto &selection = selections.packs[row];
        // Keep detail records clear of family records, ground colours and the
        // reflection table. Alpha is availability; RG stores the tile base.
        for (const auto &[column, atlas] :
             {std::pair{200, selection.cartTracks}, std::pair{201, selection.cityWallFringe}})
            if (atlas >= 0) {
                const int base = atlas * 16;
                table.setPixelColor(column,
                                    static_cast<int>(row),
                                    QColor{base & 255, (base >> 8) & 255, 0, 255});
            }
        for (std::size_t family = 0; family < selection.materials.size(); ++family) {
            const int atlas = selection.materials[family];
            const int tileBase = atlas * 16;
            table.setPixelColor(static_cast<int>(family),
                                static_cast<int>(row),
                                QColor{tileBase & 255, (tileBase >> 8) & 255, 0, 255});
            if (atlas == selections.packs.front().materials[family])
                continue;
            // A regional ground undercoat should not turn snowy behind a
            // deliberately green winter sheet. Derive its fill from the art.
            for (int season = 0; season < 4; ++season) {
                const int index = season * 6;
                const auto &page = sprites.atlases.at(static_cast<std::size_t>(atlas + index / 16))
                                       .front();
                std::array<int64_t, 4> sums{};
                for (int y = 0; y < 128; y += 4)
                    for (int x = 0; x < 128; x += 4) {
                        const auto color = page.pixelColor((index % 4) * 128 + x,
                                                           (index % 16) / 4 * 128 + y);
                        const int alpha = color.alpha();
                        if (alpha < 128)
                            continue;
                        sums[0] += color.red() * alpha;
                        sums[1] += color.green() * alpha;
                        sums[2] += color.blue() * alpha;
                        sums[3] += alpha;
                    }
                if (sums[3] != 0)
                    table.setPixelColor(32 + static_cast<int>(family) * 4 + season,
                                        static_cast<int>(row),
                                        QColor{static_cast<int>(sums[0] / sums[3]),
                                               static_cast<int>(sums[1] / sums[3]),
                                               static_cast<int>(sums[2] / sums[3]),
                                               255});
            }
        }
    }
    for (const auto &source : reflectionSources)
        for (int page = 0; page < 2; ++page) {
            const int layer = source[0] + page;
            const int data = source[1] * 16;
            table.setPixelColor(layer % 512,
                                64 + layer / 512,
                                QColor{data & 255, (data >> 8) & 255, source[2] + 1, 255});
        }
    std::vector<QImage> pages{table};
    for (int size = 256; size >= 4; size /= 2) {
        QImage mip{size, size, QImage::Format_RGBA8888};
        mip.fill(Qt::transparent);
        pages.push_back(mip);
    }
    return pages;
}
} // namespace terrain_sprites
