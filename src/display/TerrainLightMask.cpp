// SPDX-License-Identifier: GPL-2.0-or-later
#include "TerrainLightMask.h"

#include "TerrainRoomLightRules.h"

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace terrain_room_light {
MaskAtlas::MaskAtlas(const QImage &sheet)
{
    if (sheet.isNull() || sheet.width() != 512 || sheet.height() != 32)
        throw std::runtime_error("Invalid terrain light mask atlas (expected 512 x 32)");
    for (int i = 0; i < 16; ++i)
        m_frames[static_cast<size_t>(i)] = sheet.copy(i * 32, 0, 32, 32)
                                               .scaled(PIXELS,
                                                       PIXELS,
                                                       Qt::IgnoreAspectRatio,
                                                       Qt::SmoothTransformation)
                                               .convertToFormat(QImage::Format_RGBA8888);
}
QImage MaskAtlas::tile(const uint32_t seed,
                       const double seconds,
                       const int strength,
                       const int flicker) const
{
    const auto bytes = variation(seed);
    const double amount = static_cast<double>(std::clamp(flicker, 0, 100)) / 100.0;
    const double phase = static_cast<double>(bytes[2]) * 16.0 / 256.0;
    const double position = phase + (amount > 0.0 ? seconds * (3.0 + bytes[3] / 64.0) : 0.0);
    const double whole = std::floor(position);
    const int first = static_cast<int>(std::fmod(whole, 16.0));
    const double blend = position - whole;
    const double brightness = (0.82 + bytes[0] / 1700.0) * std::clamp(strength, 0, 100) / 100.0;
    QImage result(PIXELS, PIXELS, QImage::Format_RGBA8888);
    const auto &a = m_frames[static_cast<size_t>(first)];
    const auto &b = m_frames[static_cast<size_t>((first + 1) % 16)];
    // The steady frame is also seeded; flicker changes both corners and brightness.
    const auto &steady = m_frames[static_cast<size_t>(static_cast<int>(phase))];
    for (int y = 0; y < PIXELS; ++y) {
        auto *out = result.scanLine(y);
        const auto *pa = a.constScanLine(y);
        const auto *pb = b.constScanLine(y);
        const auto *ps = steady.constScanLine(y);
        for (int x = 0; x < PIXELS; ++x) {
            const int offset = x * 4;
            const double animated = pa[offset] * (1.0 - blend) + pb[offset] * blend;
            const bool border = x == 0 || y == 0 || x == PIXELS - 1 || y == PIXELS - 1;
            const double mask = border ? 0.0 : ps[offset] * (1.0 - amount) + animated * amount;
            out[offset] = 255;
            out[offset + 1] = static_cast<uchar>(168 + bytes[1] / 10);
            // Blue stores a cached corner-shadow opacity, not a colour channel.
            const double edge = static_cast<double>(
                std::min({x, y, PIXELS - 1 - x, PIXELS - 1 - y}));
            const double t = std::clamp(edge / 1.5, 0.0, 1.0);
            const double boundary = t * t * (3.0 - 2.0 * t);
            const double shade = std::max(0.0, boundary - mask / 255.0);
            out[offset + 2] = static_cast<uchar>(shade * 70.0 * std::clamp(strength, 0, 100)
                                                 / 100.0);
            out[offset + 3] = static_cast<uchar>(std::clamp(mask * brightness, 0.0, 255.0));
        }
    }
    return result;
}
} // namespace terrain_room_light
