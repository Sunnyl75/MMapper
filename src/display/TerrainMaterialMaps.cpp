// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 The MMapper Authors
#include "TerrainMaterialMaps.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <vector>

#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSaveFile>
#include <queue>

namespace terrain_material_maps {
namespace {
using Field = std::vector<float>;
float smooth(float a, float b, float value)
{
    const float t = std::clamp((value - a) / std::max(b - a, 1.e-5f), 0.f, 1.f);
    return t * t * (3.f - 2.f * t);
}
int byte(float value)
{
    return static_cast<int>(std::clamp(value, 0.f, 1.f) * 255.f + .5f);
}
Field blur(const Field &input, int size, int radius)
{
    Field tmp(input.size()), output(input.size());
    const auto index = [size](int x, int y) { return static_cast<std::size_t>(y * size + x); };
    // Sliding sums keep each Gaussian approximation pass linear in pixel count.
    Field current = input;
    const float divisor = static_cast<float>(2 * radius + 1);
    for (int pass = 0; pass < 3; ++pass) {
        for (int y = 0; y < size; ++y) {
            float sum = 0.f;
            for (int k = -radius; k <= radius; ++k)
                sum += current[index(std::clamp(k, 0, size - 1), y)];
            for (int x = 0; x < size; ++x) {
                tmp[index(x, y)] = sum / divisor;
                sum += current[index(std::min(x + radius + 1, size - 1), y)]
                       - current[index(std::max(x - radius, 0), y)];
            }
        }
        for (int x = 0; x < size; ++x) {
            float sum = 0.f;
            for (int k = -radius; k <= radius; ++k)
                sum += tmp[index(x, std::clamp(k, 0, size - 1))];
            for (int y = 0; y < size; ++y) {
                output[index(x, y)] = sum / divisor;
                sum += tmp[index(x, std::min(y + radius + 1, size - 1))]
                       - tmp[index(x, std::max(y - radius, 0))];
            }
        }
        current.swap(output);
    }
    return current;
}
float percentile(const Field &values, float fraction)
{
    if (values.empty())
        return 0.f;
    return values[static_cast<std::size_t>(fraction * static_cast<float>(values.size() - 1))];
}
QImage downsample(const QImage &input)
{
    const int size = input.width() / 2;
    QImage result(size, size, QImage::Format_ARGB32);
    for (int y = 0; y < size; ++y)
        for (int x = 0; x < size; ++x) {
            float nx = 0.f, ny = 0.f, nz = 0.f, mask = 0.f;
            for (int j = 0; j < 2; ++j)
                for (int i = 0; i < 2; ++i) {
                    const auto pixel = input.pixel(x * 2 + i, y * 2 + j);
                    nx += static_cast<float>(qRed(pixel)) / 127.5f - 1.f;
                    ny += static_cast<float>(qGreen(pixel)) / 127.5f - 1.f;
                    nz += static_cast<float>(qBlue(pixel)) / 127.5f - 1.f;
                    mask += static_cast<float>(qAlpha(pixel)) / 255.f;
                }
            const float length = std::max(std::sqrt(nx * nx + ny * ny + nz * nz), .001f);
            result.setPixel(x,
                            y,
                            qRgba(byte(nx / length * .5f + .5f),
                                  byte(ny / length * .5f + .5f),
                                  byte(nz / length * .5f + .5f),
                                  byte(mask * .25f)));
        }
    return result;
}
Pages mipmaps(const std::vector<QImage> &bases)
{
    Pages pages;
    for (const auto &base : bases) {
        std::vector<QImage> frames;
        const int cell = base.width() / 4;
        for (int i = 0; i < 16; ++i)
            frames.push_back(base.copy((i % 4) * cell, (i / 4) * cell, cell, cell)
                                 .convertToFormat(QImage::Format_ARGB32));
        std::vector<QImage> levels;
        for (int size = cell; size >= 1; size /= 2) {
            QImage atlas(size * 4, size * 4, QImage::Format_ARGB32);
            for (int i = 0; i < 16; ++i)
                for (int y = 0; y < size; ++y)
                    for (int x = 0; x < size; ++x)
                        atlas.setPixel((i % 4) * size + x,
                                       (i / 4) * size + y,
                                       frames[static_cast<std::size_t>(i)].pixel(x, y));
            levels.push_back(atlas.convertToFormat(QImage::Format_RGBA8888));
            if (size > 1)
                for (auto &frame : frames)
                    frame = downsample(frame);
        }
        pages.push_back(std::move(levels));
    }
    return pages;
}
} // namespace

QImage generate(const QImage &source, const Surface surface)
{
    if (source.isNull() || source.width() != source.height())
        return {};
    const QImage image = source.convertToFormat(QImage::Format_ARGB32);
    const int size = image.width();
    const auto count = static_cast<std::size_t>(size * size);
    Field luminance(count), alpha(count), distance(count, -1.f), selection(count);
    std::vector<int> nearest(count, -1);
    std::queue<int> fill, edge;
    bool selected = false;
    const auto index = [size](int x, int y) { return y * size + x; };
    for (int y = 0; y < size; ++y)
        for (int x = 0; x < size; ++x) {
            const auto pos = static_cast<std::size_t>(index(x, y));
            const auto p = image.pixel(x, y);
            luminance[pos] = (.2126f * static_cast<float>(qRed(p))
                              + .7152f * static_cast<float>(qGreen(p))
                              + .0722f * static_cast<float>(qBlue(p)))
                             / 255.f;
            alpha[pos] = static_cast<float>(qAlpha(p)) / 255.f;
            const float red = static_cast<float>(qRed(p)) / 255.f;
            const float green = static_cast<float>(qGreen(p)) / 255.f;
            const float blue = static_cast<float>(qBlue(p)) / 255.f;
            const float minimum = std::min({red, green, blue});
            const float chroma = std::max({red, green, blue}) - minimum;
            // Wet grey/blue pools versus olive vegetation and brown mud.
            const float wet = smooth(.72f, .95f, blue / std::max(red, .01f))
                              * smooth(.68f, .87f, blue / std::max(green, .01f));
            const float white = smooth(.57f, .87f, minimum) * (1.f - smooth(.09f, .22f, chroma));
            switch (surface) {
            case Surface::WATER:
                selection[pos] = 1.f;
                break;
            case Surface::SHALLOWS:
                // Seasonal pools become olive/tea coloured. Low saturation
                // separates them from the yellow-green reeds and brown mud.
                selection[pos] = (1.f
                                  - smooth(.32f, .62f, chroma / std::max({red, green, blue, .01f})))
                                 * smooth(.09f, .20f, luminance[pos])
                                 * (1.f - smooth(.48f, .68f, luminance[pos]));
                break;
            case Surface::RAPIDS:
                selection[pos] = std::max(wet * .60f, white * .45f);
                break;
            case Surface::MOUNTAIN:
                selection[pos] = white;
                break;
            }
            if (alpha[pos] > .85f) {
                selected |= selection[pos] > .001f;
                nearest[pos] = static_cast<int>(pos);
                fill.push(static_cast<int>(pos));
            }
            if (alpha[pos] <= .85f || x == 0 || y == 0 || x == size - 1 || y == size - 1) {
                distance[pos] = 0.f;
                edge.push(static_cast<int>(pos));
            }
        }
    const auto spread = [&](std::queue<int> &queue, bool distances) {
        while (!queue.empty()) {
            const int at = queue.front();
            queue.pop();
            const int x = at % size, y = at / size;
            const int dx[4]{-1, 1, 0, 0}, dy[4]{0, 0, -1, 1};
            for (int k = 0; k < 4; ++k) {
                const int xx = x + dx[k], yy = y + dy[k];
                if (xx < 0 || yy < 0 || xx >= size || yy >= size)
                    continue;
                const auto next = static_cast<std::size_t>(index(xx, yy));
                if (distances ? distance[next] >= 0.f : nearest[next] >= 0)
                    continue;
                if (distances)
                    distance[next] = distance[static_cast<std::size_t>(at)] + 1.f;
                else
                    nearest[next] = nearest[static_cast<std::size_t>(at)];
                queue.push(static_cast<int>(next));
            }
        }
    };
    if (fill.empty() || (surface != Surface::WATER && !selected)) {
        QImage empty(size, size, QImage::Format_ARGB32);
        empty.fill(qRgba(128, 128, 255, 0));
        return empty.convertToFormat(QImage::Format_RGBA8888);
    }
    spread(fill, false);
    spread(edge, true);
    for (std::size_t i = 0; i < count; ++i)
        if (nearest[i] >= 0)
            luminance[i] = luminance[static_cast<std::size_t>(nearest[i])];
    const auto [darkest, brightest] = std::minmax_element(luminance.begin(), luminance.end());
    if (surface == Surface::WATER && *brightest - *darkest < 1.f / 255.f) {
        QImage flat(size, size, QImage::Format_ARGB32);
        flat.fill(qRgba(128, 128, 255, 0));
        return flat.convertToFormat(QImage::Format_RGBA8888);
    }
    const auto small = blur(luminance, size, 1), broad = blur(luminance, size, 4);
    Field height = small;
    if (surface == Surface::SHALLOWS) {
        Field grain(count);
        for (std::size_t i = 0; i < count; ++i) {
            uint32_t hash = static_cast<uint32_t>(i) ^ 0x4d415253u;
            hash = (hash ^ (hash >> 16u)) * 0x7feb352du;
            hash = (hash ^ (hash >> 15u)) * 0x846ca68bu;
            grain[i] = static_cast<float>((hash ^ (hash >> 16u)) & 65535u) / 65535.f;
        }
        const auto texture = blur(grain, size, 1);
        // Static micro-relief only in wet areas. No animated noise or new waves.
        for (std::size_t i = 0; i < count; ++i)
            height[i] += selection[i] * (texture[i] - .5f) * .35f;
    }
    Field detail(count), values, brightness;
    for (std::size_t i = 0; i < count; ++i) {
        detail[i] = small[i] - broad[i];
        if (alpha[i] > .85f) {
            values.push_back(detail[i]);
            brightness.push_back(luminance[i]);
        }
    }
    std::sort(values.begin(), values.end());
    std::sort(brightness.begin(), brightness.end());
    const float low = percentile(values, .35f), high = percentile(values, .88f);
    const float dark = percentile(brightness, .12f), light = percentile(brightness, .76f);
    QImage output(size, size, QImage::Format_ARGB32);
    for (int y = 0; y < size; ++y)
        for (int x = 0; x < size; ++x) {
            const auto at = static_cast<std::size_t>(index(x, y));
            const float gx = (height[static_cast<std::size_t>(index(std::min(x + 1, size - 1), y))]
                              - height[static_cast<std::size_t>(index(std::max(x - 1, 0), y))])
                             * 6.f;
            const float gy = (height[static_cast<std::size_t>(index(x, std::min(y + 1, size - 1)))]
                              - height[static_cast<std::size_t>(index(x, std::max(y - 1, 0)))])
                             * 6.f;
            const float length = std::sqrt(gx * gx + gy * gy + 1.f);
            float material = smooth(low, high, detail[at]) * smooth(dark, light, luminance[at]);
            if (surface == Surface::SHALLOWS)
                material = selection[at] * (.28f + .16f * smooth(dark, light, luminance[at]));
            else if (surface == Surface::RAPIDS)
                material = selection[at] * (.55f + .45f * smooth(low, high, detail[at]));
            else if (surface == Surface::MOUNTAIN)
                material = selection[at] * .80f;
            const float mask = material * smooth(.25f, .7f, alpha[at])
                               * smooth(1.f, 6.f, distance[at]);
            output.setPixel(x,
                            y,
                            qRgba(byte(-gx / length * .5f + .5f),
                                  byte(-gy / length * .5f + .5f),
                                  byte(1.f / length * .5f + .5f),
                                  byte(mask)));
        }
    return output.convertToFormat(QImage::Format_RGBA8888);
}

Pages neutralPages()
{
    QImage base(512, 512, QImage::Format_ARGB32);
    base.fill(qRgba(128, 128, 255, 0));
    return mipmaps({base, base});
}

Pages loadOrGenerate(const Pages &colour, const QString &cacheDirectory, const Surface surface)
{
    if (colour.size() != 2 || colour[0].empty() || colour[1].empty())
        return neutralPages();
    QCryptographicHash hash(QCryptographicHash::Sha256);
    hash.addData(QByteArrayLiteral("terrain-material-maps-v2"));
    hash.addData(QByteArray::number(static_cast<int>(surface)));
    for (const auto &page : colour) {
        const auto image = page.front().convertToFormat(QImage::Format_RGBA8888);
        if (image.size() != QSize(512, 512))
            return neutralPages();
        for (int y = 0; y < image.height(); ++y)
            hash.addData(QByteArrayView(reinterpret_cast<const char *>(image.constScanLine(y)),
                                        image.width() * 4));
    }
    const QString key = QString::fromLatin1(hash.result().toHex());
    QDir cache(cacheDirectory);
    std::vector<QImage> bases;
    if (!cacheDirectory.isEmpty()) {
        for (int page = 0; page < 2; ++page) {
            QImage image(cache.filePath(key + QString::number(page) + ".png"));
            if (image.size() != QSize(512, 512))
                break;
            bases.push_back(image.convertToFormat(QImage::Format_RGBA8888));
        }
        if (bases.size() == 2)
            return mipmaps(bases);
    }
    bases.clear();
    for (const auto &page : colour) {
        QImage atlas(512, 512, QImage::Format_ARGB32);
        for (int tile = 0; tile < 16; ++tile) {
            const auto frame
                = generate(page.front().copy((tile % 4) * 128, (tile / 4) * 128, 128, 128), surface);
            for (int y = 0; y < 128; ++y)
                for (int x = 0; x < 128; ++x)
                    atlas.setPixel((tile % 4) * 128 + x, (tile / 4) * 128 + y, frame.pixel(x, y));
        }
        bases.push_back(atlas.convertToFormat(QImage::Format_RGBA8888));
    }
    if (!cacheDirectory.isEmpty() && cache.mkpath(".")) {
        for (std::size_t i = 0; i < bases.size(); ++i) {
            QSaveFile file(cache.filePath(key + QString::number(i) + ".png"));
            if (file.open(QIODevice::WriteOnly) && bases[i].save(&file, "PNG"))
                static_cast<void>(file.commit());
        }
        const auto files = cache.entryInfoList({"*.png"}, QDir::Files, QDir::Time);
        for (int i = 32; i < files.size(); ++i)
            static_cast<void>(QFile::remove(files[i].absoluteFilePath()));
    }
    return mipmaps(bases);
}
} // namespace terrain_material_maps
