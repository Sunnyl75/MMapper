#pragma once
// SPDX-License-Identifier: GPL-2.0-or-later
#include <array>
#include <cstdint>

#include <QImage>

// CPU-side animation. The GPU only samples the resulting cached texture.
namespace terrain_room_light {
class MaskAtlas final
{
    std::array<QImage, 16> m_frames;

public:
    static constexpr int PIXELS = 16;
    explicit MaskAtlas(const QImage &sheet);
    QImage tile(uint32_t seed, double seconds, int strength, int flicker) const;
};
} // namespace terrain_room_light
