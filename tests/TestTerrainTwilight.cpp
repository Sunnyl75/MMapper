// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 The MMapper Authors
#include "../src/display/TerrainTwilight.h"

#include <cassert>
#include <cmath>
#include <iostream>

int main()
{
    using namespace terrain_twilight;
    const Tint night{0.05f, 0.05f, 0.2f, 0.35f};
    for (const auto dawn : {4.0, 6.0, 9.0})
        for (const auto dusk : {16.0, 18.0, 22.0}) {
            assert(sample(dawn - 1.0, dawn, dusk, night).changing);
            assert(!sample(dawn, dawn, dusk, night).changing);
            assert(!sample(dusk, dawn, dusk, night).changing);
            assert(std::abs(sample(dawn, dawn, dusk, night).tint[3]) < 1.e-6f);
            assert(std::abs(sample(dusk, dawn, dusk, night).tint[3] - night[3]) < 1.e-6f);
            float lastDawn = 1.f, lastDusk = 0.f;
            for (int i = 0; i <= 3600; ++i) {
                const double offset = static_cast<double>(i) / 3600.0;
                const auto a = sample(dawn - 1.0 + offset, dawn, dusk, night).tint;
                const auto b = sample(dusk - 1.0 + offset, dawn, dusk, night).tint;
                assert(a[3] <= lastDawn + 1.e-6f && b[3] + 1.e-6f >= lastDusk);
                for (std::size_t c = 0; c < 4; ++c) {
                    assert(std::isfinite(a[c]) && a[c] >= 0.f && a[c] <= 1.f);
                    assert(std::isfinite(b[c]) && b[c] >= 0.f && b[c] <= 1.f);
                }
                lastDawn = a[3];
                lastDusk = b[3];
            }
            // Continuity of visible colour/alpha, including transparent ends,
            // palette knots, dawn/dusk and the midnight/day wrap.
            for (const auto hour : {0.0,
                                    dawn - 1.0,
                                    dawn - .8,
                                    dawn - .6,
                                    dawn - .4,
                                    dawn - .2,
                                    dawn,
                                    dusk - 1.0,
                                    dusk - .8,
                                    dusk - .6,
                                    dusk - .4,
                                    dusk - .2,
                                    dusk,
                                    24.0}) {
                const auto left = sample(hour - 1.e-6, dawn, dusk, night).tint;
                const auto right = sample(hour + 1.e-6, dawn, dusk, night).tint;
                assert(std::abs(left[3] - right[3]) < 1.e-5f);
                for (std::size_t c = 0; c < 3; ++c)
                    assert(std::abs(left[c] * left[3] - right[c] * right[3]) < 1.e-5f);
            }
        }
    assert(!sample(5.0, 10.0, 6.0, night).changing);
    for (int i = 0; i < 1440; ++i)
        assert(std::abs(sample(i / 60.0, 6.0, 18.0, {0.f, 0.f, 0.f, 0.f}).tint[3]) < 1.e-6f);
    std::cout << "Twilight colours, opacity, boundaries and continuous wrap passed\n";
}
