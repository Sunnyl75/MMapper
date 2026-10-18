#pragma once
// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 The MMapper Authors

#include <algorithm>
#include <cmath>
#include <numbers>

// An artistic sky model tied to MUME's clock, not Earth's calendar or time zone.
namespace terrain_lighting {
inline constexpr double LATITUDE = 52.5 * std::numbers::pi / 180.0;
inline constexpr double MAX_LENGTH = 1.0;

struct Shadow final
{
    float x = 0.f;
    float y = 0.f;
    float opacity = 0.f;
};

namespace detail {
inline Shadow project(const double hourAngle, const double declination, const double opacity)
{
    const double altitude = std::sin(LATITUDE) * std::sin(declination)
                            + std::cos(LATITUDE) * std::cos(declination) * std::cos(hourAngle);
    if (altitude <= 0.0) {
        return {};
    }
    const double east = -std::cos(declination) * std::sin(hourAngle);
    const double north = std::cos(LATITUDE) * std::sin(declination)
                         - std::sin(LATITUDE) * std::cos(declination) * std::cos(hourAngle);
    const double horizontal = std::hypot(east, north);
    if (horizontal < 1.e-9) {
        return {};
    }
    const double length = std::min(horizontal / altitude, MAX_LENGTH);
    const double strength = opacity * std::clamp(altitude / 0.15, 0.0, 1.0);
    return Shadow{static_cast<float>(-east / horizontal * length),
                  static_cast<float>(-north / horizontal * length),
                  static_cast<float>(strength)};
}
} // namespace detail

inline Shadow sun(const double hour, const double dawn, const double dusk)
{
    if (!(dawn < dusk && dusk - dawn < 24.0 && hour > dawn && hour < dusk)) {
        return {};
    }
    // Solve the horizon equation for an apparent seasonal declination, so
    // the sun reaches the horizon at the game's existing dawn/dusk times.
    const double sunsetAngle = (dusk - dawn) * std::numbers::pi / 24.0;
    const double declination = std::atan(-std::cos(sunsetAngle) / std::tan(LATITUDE));
    const double angle = (hour - (dawn + dusk) / 2.0) * std::numbers::pi / 12.0;
    return detail::project(angle, declination, 0.18);
}

inline Shadow calculate(const double hour,
                        const double dawn,
                        const double dusk,
                        const double moonZenith,
                        const double moonLight,
                        const bool moonVisible)
{
    if (hour > dawn && hour < dusk) {
        return sun(hour, dawn, dusk);
    }
    if (!moonVisible || moonLight <= 0.0 || !(dawn < dusk)) {
        return {};
    }
    const double offset = std::remainder(hour - moonZenith, 24.0);
    if (std::abs(offset) >= 6.0) {
        return {};
    }
    // MMapper's moon rises six game hours before zenith and sets six after;
    // zero declination preserves that existing twelve-hour sky arc.
    const double light = std::clamp(moonLight, 0.0, 1.0);
    auto result = detail::project(offset * std::numbers::pi / 12.0, 0.0, 0.045 * light * light);
    const double nightHour = hour < dawn ? hour + 24.0 : hour;
    const double twilight = std::min(nightHour - dusk, dawn + 24.0 - nightHour);
    result.opacity *= static_cast<float>(std::clamp(twilight / 0.5, 0.0, 1.0));
    return result;
}
} // namespace terrain_lighting
