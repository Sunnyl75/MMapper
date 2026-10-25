// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 The MMapper Authors
#include "display/TerrainLighting.h"
#include "display/TerrainReflections.h"

#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>

namespace {
int checks = 0;
void require(const bool condition)
{
    if (!condition)
        throw std::runtime_error("Terrain moonlight regression");
    ++checks;
}
bool near(const float a, const float b)
{
    return std::abs(a - b) < 1.e-5f;
}
} // namespace

int main()
{
    using terrain_reflections::sample;
    const auto full = sample(0, 6, 18, 0, 1, true);
    require(full.moonlight > .99f && full.direction[3] > .12f && full.direction[3] < .2f);
    require(full.color[2] > full.color[1] && full.color[1] > full.color[0]);
    const auto half = sample(0, 6, 18, 0, .5, true);
    require(near(half.moonlight, full.moonlight * .25f));
    require(near(half.direction[3], full.direction[3] * .25f));
    require(sample(0, 6, 18, 0, 0, true).moonlight < 1.e-6f);
    require(sample(0, 6, 18, 0, 1, false).moonlight < 1.e-6f);
    require(sample(0, 6, 18, 12, 1, true).moonlight < 1.e-6f);
    require(sample(0, 6, 18, 6, 1, true).moonlight < 1.e-6f);
    const auto low = sample(0, 6, 18, 5.95, 1, true);
    require(low.moonlight > 0.f && low.moonlight < .02f);

    // Direction follows the same equatorial moon arc used by existing shadows.
    for (const double hour : {20.0, 22.0, 0.0, 2.0, 4.0}) {
        const auto moon = sample(hour, 6, 18, 0, 1, true);
        const auto shadow = terrain_lighting::calculate(hour, 6, 18, 0, 1, true);
        require(moon.direction[0] * -shadow.x + moon.direction[1] * -shadow.y > 0.f);
        const float length = std::hypot(moon.direction[0], moon.direction[1], moon.direction[2]);
        require(near(length, 1.f));
    }
    require(sample(20, 6, 18, 0, 1, true).direction[0] > 0.f);
    require(sample(4, 6, 18, 0, 1, true).direction[0] < 0.f);

    // Native warning hours retain the solar source and accepted twilight hues.
    for (const double hour : {6.0, 6.5, 7.0, 12.0, 18.0, 18.5}) {
        const auto solarSample = sample(hour, 6, 18, 0, 1, true);
        require(solarSample.moonlight < 1.e-6f);
        require(solarSample.direction[3] > 0.f);
    }
    require(sample(19, 6, 18, 21, 1, true).moonlight < 1.e-6f);
    require(sample(19.1, 6, 18, 21, 1, true).moonlight < sample(19.4, 6, 18, 21, 1, true).moonlight);
    require(sample(5.9, 6, 18, 3, 1, true).moonlight < sample(5.6, 6, 18, 3, 1, true).moonlight);

    for (const double dawn : {4.0, 6.0, 8.0})
        for (const double dusk : {16.0, 19.0, 22.0})
            for (int minute = 0; minute < 1440; ++minute) {
                const auto moon
                    = sample(static_cast<double>(minute) / 60.0, dawn, dusk, 0, .75, true);
                require(std::isfinite(moon.moonlight) && moon.moonlight >= 0.f
                        && moon.moonlight <= 1.f);
            }
    const auto invalid = sample(std::numeric_limits<double>::quiet_NaN(), 6, 18, 0, 1, true);
    require(invalid.moonlight < 1.e-6f && invalid.direction[3] < 1.e-6f);
    require(sample(0, 6, 18, std::numeric_limits<double>::infinity(), 1, true).moonlight < 1.e-6f);
    std::cout << "PASS " << checks << " terrain moonlight assertions\n";
}
