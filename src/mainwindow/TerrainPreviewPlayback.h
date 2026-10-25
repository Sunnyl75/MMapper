#pragma once
// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 The MMapper Authors
#include <algorithm>
#include <cmath>
#include <cstdint>

namespace terrain_preview {
inline double rate(int position)
{
    position = std::clamp(position, -100, 100);
    // Centre = +1x. First step left = -1x. Both extremes reach 1024x.
    return position < 0 ? -std::exp2(static_cast<double>(-position - 1) * 10.0 / 99.0)
                        : std::exp2(static_cast<double>(position) / 10.0);
}
class Playback final
{
    double m_fraction = 0.0;

public:
    void reset() { m_fraction = 0.0; }
    int64_t advance(int64_t milliseconds, int position)
    {
        m_fraction += static_cast<double>(milliseconds) / 1000.0 * rate(position);
        const auto whole = static_cast<int64_t>(m_fraction);
        m_fraction -= static_cast<double>(whole);
        return whole;
    }
};
} // namespace terrain_preview
