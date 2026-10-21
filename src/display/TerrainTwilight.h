#pragma once
// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 The MMapper Authors

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>

// Map-lighting artwork, in MUME hours. Night colour/alpha come from the
// existing weather preference; no Earth calendar or fixed wall-clock hours.
namespace terrain_twilight {
using Tint = std::array<float, 4>;
struct Sample final
{
    Tint tint{};
    bool changing = false;
    Tint highlights{}; // Overlay-blend colour/strength; separate from night opacity.
};
inline float linear(const float value)
{
    return value <= 0.04045f ? value / 12.92f : std::pow((value + 0.055f) / 1.055f, 2.4f);
}
inline float srgb(const float value)
{
    return std::clamp(value <= 0.0031308f ? value * 12.92f
                                          : 1.055f * std::pow(value, 1.f / 2.4f) - 0.055f,
                      0.f,
                      1.f);
}
inline Tint blend(const Tint &a, const Tint &b, const float progress)
{
    const float t = progress * progress * (3.f - 2.f * progress);
    Tint result{};
    result[3] = a[3] * (1.f - t) + b[3] * t;
    if (result[3] > 0.f)
        for (std::size_t i = 0; i < 3; ++i)
            result[i] = srgb((linear(a[i]) * a[3] * (1.f - t) + linear(b[i]) * b[3] * t)
                             / result[3]);
    return result;
}
inline Sample sample(double hour, const double dawn, const double dusk, const Tint &night)
{
    if (!std::isfinite(hour) || !std::isfinite(dawn) || !std::isfinite(dusk) || dawn < 1.0
        || dusk > 24.0 || dawn >= dusk)
        return {};
    hour = std::fmod(std::fmod(hour, 24.0) + 24.0, 24.0);
    const bool sunrise = hour >= dawn - 1.0 && hour < dawn;
    const bool sunset = hour >= dusk - 1.0 && hour < dusk;
    if (!sunrise && !sunset)
        return {hour < dawn || hour >= dusk ? night : Tint{}, false};
    const float alpha = night[3];
    const std::array<Tint, 6> dawnStops{night,
                                        Tint{0.30f, 0.27f, 0.51f, alpha * 0.80f},
                                        Tint{0.78f, 0.40f, 0.53f, alpha * 0.55f},
                                        Tint{1.f, 0.65f, 0.41f, alpha * 0.30f},
                                        Tint{1.f, 0.83f, 0.52f, alpha * 0.10f},
                                        Tint{}};
    const std::array<Tint, 6> duskStops{Tint{},
                                        Tint{1.f, 0.75f, 0.34f, alpha * 0.10f},
                                        Tint{1.f, 0.54f, 0.26f, alpha * 0.20f},
                                        Tint{0.76f, 0.34f, 0.49f, alpha * 0.38f},
                                        Tint{0.32f, 0.25f, 0.47f, alpha * 0.70f},
                                        night};
    const auto &stops = sunrise ? dawnStops : duskStops;
    const double scaled = (hour - ((sunrise ? dawn : dusk) - 1.0)) * 5.0;
    const auto segment = std::min(static_cast<std::size_t>(scaled), stops.size() - 2);
    const float progress = static_cast<float>(scaled - static_cast<double>(segment));
    const auto tint = blend(stops[segment], stops[segment + 1], progress);
    // A smooth bell fades the highlight grade out at both ends of the hour.
    // Unlike the sky wash, its strength does not vanish at the golden stops.
    const float phase = static_cast<float>(scaled / 5.0);
    const float envelope = 16.f * phase * phase * (1.f - phase) * (1.f - phase);
    const float strength = 0.80f * envelope * std::clamp(alpha / 0.35f, 0.f, 1.f);
    const Tint highlights{0.5f + 0.4f * (tint[0] - 0.5f),
                          0.5f + 0.4f * (tint[1] - 0.5f),
                          0.5f + 0.4f * (tint[2] - 0.5f),
                          strength};
    return {tint, true, highlights};
}
} // namespace terrain_twilight
