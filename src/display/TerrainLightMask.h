#pragma once
// SPDX-License-Identifier: GPL-2.0-or-later
#include <array>
#include <cstdint>

#include <QImage>

// CPU-side animation. The GPU only samples the resulting cached texture.
namespace terrain_room_light {
// NESW walls in the source room and its eight neighbours. Unknown rooms are
// solid; a boundary blocks when either side records a wall or a static door.
struct WallNeighborhood final
{
    std::array<uint8_t, 9> walls{15, 15, 15, 15, 15, 15, 15, 15, 15};
};
class MaskAtlas final
{
    std::array<QImage, 16> m_frames;

public:
    static constexpr int PIXELS = 16; // Mask texture density per map room.
    static constexpr int HALO_PIXELS = 32;
    static constexpr int HALO_OFFSET = 8;
    using Clip = std::array<uint32_t, HALO_PIXELS>;
    explicit MaskAtlas(const QImage &sheet);
    static Clip clip(const WallNeighborhood &neighbours);
    QImage tile(uint32_t seed, double seconds, int strength, int flicker) const;
    static void applyClip(QImage &tile, const Clip &clip);
    static void composite(QImage &image, const QImage &tile, int x, int y);
};
} // namespace terrain_room_light
