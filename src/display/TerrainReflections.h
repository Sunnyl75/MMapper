#pragma once
// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 The MMapper Authors
#include <algorithm>
#include <array>
#include <cmath>
#include <numbers>

namespace terrain_reflections {
struct Light final
{
    std::array<float, 4> direction{0.f, 0.f, 1.f, 0.f};
    std::array<float, 4> color{1.f, 1.f, 1.f, 1.f};
    float moonlight = 0.f; // Shared phase/horizon envelope for diffuse moonlight.
};
inline Light sample(double hour,
                    double dawnWarning,
                    double duskWarning,
                    double moonZenith,
                    double moonLevel,
                    bool moonVisible)
{
    constexpr double pi = std::numbers::pi;
    constexpr double latitude = 52.5 * pi / 180.0;
    Light result;
    double angle = 0.0, declination = 0.0, strength = 0.0;
    // Native DAWN/DUSK are the hour-long warnings, not shifted clock hours.
    const double sunrise = dawnWarning + 1.0, sunset = duskWarning + 1.0;
    if (!std::isfinite(hour) || !std::isfinite(dawnWarning) || !std::isfinite(duskWarning)
        || !(dawnWarning < duskWarning))
        return result;
    if (hour >= dawnWarning && hour < sunset) {
        declination = std::atan(-std::cos((sunset - sunrise) * pi / 24.0) / std::tan(latitude));
        angle = (hour - (sunrise + sunset) / 2.0) * pi / 12.0;
        const double warningProgress = std::clamp(hour - dawnWarning, 0.0, 1.0);
        // A dawn sky can catch water before the sun itself reaches the horizon.
        strength = .15 + .85 * warningProgress;
        const double sunsetProgress = std::clamp(hour - duskWarning, 0.0, 1.0);
        strength *= 1.0 - .92 * sunsetProgress * sunsetProgress * (3.0 - 2.0 * sunsetProgress);
        const double warmth = std::clamp(std::max(sunrise + .5 - hour, hour - duskWarning),
                                         0.0,
                                         1.0);
        result.color = {1.f,
                        static_cast<float>(.96 - .42 * warmth),
                        static_cast<float>(.88 - .57 * warmth),
                        1.f};
    } else {
        if (!moonVisible || !std::isfinite(moonZenith) || !std::isfinite(moonLevel)
            || moonLevel <= 0.0)
            return result;
        const double offset = std::remainder(hour - moonZenith, 24.0);
        if (std::abs(offset) >= 6.0)
            return result;
        angle = offset * pi / 12.0;
        const double phase = std::clamp(moonLevel, 0.0, 1.0);
        const double altitude = std::cos(latitude) * std::cos(angle);
        const double horizon = std::clamp((6.0 - std::abs(offset)) / .5, 0.0, 1.0)
                               * std::clamp(altitude / .15, 0.0, 1.0);
        const double nightHour = hour < dawnWarning ? hour + 24.0 : hour;
        const double nightEdge = std::min(nightHour - sunset, dawnWarning + 24.0 - nightHour);
        const double fade = std::clamp(nightEdge / .5, 0.0, 1.0);
        // Fade before the native DAWN warning and after DUSK becomes NIGHT.
        // The accepted one-hour dawn/dusk palettes and solar branch stay intact.
        result.moonlight = static_cast<float>(phase * phase * horizon * fade * fade
                                              * (3.0 - 2.0 * fade));
        strength = .18 * static_cast<double>(result.moonlight);
        result.color = {.62f, .76f, 1.f, 1.f};
    }
    const double east = -std::cos(declination) * std::sin(angle);
    const double north = std::cos(latitude) * std::sin(declination)
                         - std::sin(latitude) * std::cos(declination) * std::cos(angle);
    const double up = std::sin(latitude) * std::sin(declination)
                      + std::cos(latitude) * std::cos(declination) * std::cos(angle);
    const double length = std::sqrt(east * east + north * north + up * up);
    result.direction = {static_cast<float>(east / length),
                        static_cast<float>(north / length),
                        static_cast<float>(up / length),
                        static_cast<float>(strength)};
    return result;
}
} // namespace terrain_reflections
