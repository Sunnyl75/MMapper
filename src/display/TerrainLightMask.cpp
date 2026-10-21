// SPDX-License-Identifier: GPL-2.0-or-later
#include "TerrainLightMask.h"

#include "TerrainRoomLightRules.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace terrain_room_light {
MaskAtlas::MaskAtlas(const QImage &sheet)
{
    if (sheet.isNull() || sheet.width() != 512 || sheet.height() != 32)
        throw std::runtime_error("Invalid terrain light mask atlas (expected 512 x 32)");
    for (int i = 0; i < 16; ++i)
        m_frames[static_cast<size_t>(i)] = sheet.copy(i * 32, 0, 32, 32)
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
    const double position = phase + (amount > 0.0 ? seconds * (6.0 + bytes[3] / 32.0) : 0.0);
    const double whole = std::floor(position);
    const int first = static_cast<int>(std::fmod(whole, 16.0));
    const double blend = position - whole;
    // Sample one atlas point per room to strengthen its brightness variation;
    // no extra fragment arithmetic or faster redraw timer is needed.
    const double centre = m_frames[static_cast<size_t>(first)].constScanLine(
                              HALO_PIXELS / 2)[HALO_PIXELS / 2 * 4]
                              * (1.0 - blend)
                          + m_frames[static_cast<size_t>((first + 1) % 16)].constScanLine(
                                HALO_PIXELS / 2)[HALO_PIXELS / 2 * 4]
                                * blend;
    const double pulse = 1.0 + amount * (centre / 255.0 - 0.92) * 2.5;
    const double brightness = (0.82 + bytes[0] / 1700.0) * std::clamp(strength, 0, 100) / 100.0
                              * pulse;
    QImage result(HALO_PIXELS, HALO_PIXELS, QImage::Format_RGBA8888);
    const auto &a = m_frames[static_cast<size_t>(first)];
    const auto &b = m_frames[static_cast<size_t>((first + 1) % 16)];
    // The steady frame is also seeded; flicker changes both corners and brightness.
    const auto &steady = m_frames[static_cast<size_t>(static_cast<int>(phase))];
    for (int y = 0; y < HALO_PIXELS; ++y) {
        auto *out = result.scanLine(y);
        const auto *pa = a.constScanLine(y);
        const auto *pb = b.constScanLine(y);
        const auto *ps = steady.constScanLine(y);
        for (int x = 0; x < HALO_PIXELS; ++x) {
            const int offset = x * 4;
            const double animated = pa[offset] * (1.0 - blend) + pb[offset] * blend;
            const bool border = x == 0 || y == 0 || x == HALO_PIXELS - 1 || y == HALO_PIXELS - 1;
            const double mask = border ? 0.0 : ps[offset] * (1.0 - amount) + animated * amount;
            out[offset] = 255;
            out[offset + 1] = static_cast<uchar>(168 + bytes[1] / 10);
            // Blue stores a cached corner-shadow opacity, not a colour channel.
            // Corner shadow belongs to the source room, not the exterior halo.
            // The animated atlas varies here as well as along the free outer rim.
            const bool inside = x >= HALO_OFFSET && x < HALO_OFFSET + PIXELS && y >= HALO_OFFSET
                                && y < HALO_OFFSET + PIXELS;
            const double shade = inside ? 1.0 - mask / 255.0 : 0.0;
            out[offset + 2] = static_cast<uchar>(shade * 70.0 * std::clamp(strength, 0, 100)
                                                 / 100.0);
            out[offset + 3] = static_cast<uchar>(std::clamp(mask * brightness, 0.0, 255.0));
        }
    }
    return result;
}

namespace {
uint8_t wallsAt(const WallNeighborhood &n, const int x, const int y)
{
    if (x < -1 || x > 1 || y < -1 || y > 1)
        return 15;
    return n.walls[static_cast<size_t>((y + 1) * 3 + x + 1)];
}
bool blocked(const WallNeighborhood &n, const int x, const int y, const int dir)
{
    constexpr std::array<int, 4> dx{0, 0, 1, -1}, dy{1, -1, 0, 0};
    return (wallsAt(n, x, y) & (1u << dir)) != 0
           || (wallsAt(n, x + dx[static_cast<size_t>(dir)], y + dy[static_cast<size_t>(dir)])
               & (1u << (dir ^ 1)))
                  != 0;
}
bool reachable(const WallNeighborhood &n, const double px, const double py)
{
    const int targetX = static_cast<int>(std::floor(px));
    const int targetY = static_cast<int>(std::floor(py));
    // Leave a zero guard inside a blocking edge. Linear texture filtering then
    // cannot leak half a texel through an otherwise perfectly clipped wall.
    constexpr double guard = 1.0 / MaskAtlas::PIXELS;
    const double fx = px - std::floor(px), fy = py - std::floor(py);
    if ((fx < guard && blocked(n, targetX, targetY, 3))
        || (fx > 1.0 - guard && blocked(n, targetX, targetY, 2))
        || (fy < guard && blocked(n, targetX, targetY, 1))
        || (fy > 1.0 - guard && blocked(n, targetX, targetY, 0)))
        return false;
    const double dx = px - 0.5, dy = py - 0.5;
    const int stepX = dx > 0.0 ? 1 : -1, stepY = dy > 0.0 ? 1 : -1;
    const int dirX = dx > 0.0 ? 2 : 3, dirY = dy > 0.0 ? 0 : 1;
    const double infinity = std::numeric_limits<double>::infinity();
    const double tx = std::abs(dx) > 1.e-12 ? 0.5 / std::abs(dx) : infinity;
    const double ty = std::abs(dy) > 1.e-12 ? 0.5 / std::abs(dy) : infinity;
    int x = 0, y = 0;
    if (targetX != 0 && targetY != 0) {
        // A ray exactly through a corner must have both routes clear; otherwise
        // a diagonal pinhole would bypass one of the two dividing walls.
        if (std::abs(tx - ty) < 1.e-9)
            return !blocked(n, 0, 0, dirX) && !blocked(n, 0, 0, dirY) && !blocked(n, stepX, 0, dirY)
                   && !blocked(n, 0, stepY, dirX);
        if (tx < ty) {
            if (blocked(n, x, y, dirX))
                return false;
            x += stepX;
        } else {
            if (blocked(n, x, y, dirY))
                return false;
            y += stepY;
        }
    }
    if (x != targetX && blocked(n, x, y, dirX))
        return false;
    if (x != targetX)
        x += stepX;
    if (y != targetY && blocked(n, x, y, dirY))
        return false;
    return true;
}
} // namespace

MaskAtlas::Clip MaskAtlas::clip(const WallNeighborhood &neighbours)
{
    Clip result{};
    for (int y = 0; y < HALO_PIXELS; ++y)
        for (int x = 0; x < HALO_PIXELS; ++x)
            if (reachable(neighbours,
                          (static_cast<double>(x - HALO_OFFSET) + 0.5) / PIXELS,
                          (static_cast<double>(y - HALO_OFFSET) + 0.5) / PIXELS))
                result[static_cast<size_t>(y)] |= uint32_t{1} << x;
    return result;
}
void MaskAtlas::applyClip(QImage &tile, const Clip &clip)
{
    for (int y = 0; y < HALO_PIXELS; ++y) {
        auto *out = tile.scanLine(y);
        for (int x = 0; x < HALO_PIXELS; ++x)
            if ((clip[static_cast<size_t>(y)] & (uint32_t{1} << x)) == 0) {
                out[x * 4 + 2] = 0;
                out[x * 4 + 3] = 0;
            }
    }
}
void MaskAtlas::composite(QImage &image, const QImage &tile, const int x, const int y)
{
    for (int sy = std::max(0, -y); sy < std::min(tile.height(), image.height() - y); ++sy) {
        const auto *source = tile.constScanLine(sy);
        auto *dest = image.scanLine(y + sy);
        for (int sx = std::max(0, -x); sx < std::min(tile.width(), image.width() - x); ++sx) {
            const int a = sx * 4, b = (x + sx) * 4;
            if (source[a + 3] > dest[b + 3]) {
                for (int c = 0; c < 4; ++c)
                    dest[b + c] = source[a + c];
            } else if (source[a + 3] == dest[b + 3]) {
                dest[b + 1] = std::max(dest[b + 1], source[a + 1]);
                dest[b + 2] = source[a + 3] == 0 ? std::max(dest[b + 2], source[a + 2])
                                                 : std::min(dest[b + 2], source[a + 2]);
            }
        }
    }
}
} // namespace terrain_room_light
