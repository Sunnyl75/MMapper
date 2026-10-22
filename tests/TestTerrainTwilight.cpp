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
            assert(!sample(dawn - 1.0, dawn, dusk, night).changing);
            assert(!sample(dawn - 1.e-6, dawn, dusk, night).changing);
            assert(sample(dawn, dawn, dusk, night).changing);
            assert(sample(dawn + 1.0 - 1.e-6, dawn, dusk, night).changing);
            assert(!sample(dawn + 1.0, dawn, dusk, night).changing);
            assert(!sample(dusk - 1.0, dawn, dusk, night).changing);
            assert(!sample(dusk - 1.e-6, dawn, dusk, night).changing);
            assert(sample(dusk, dawn, dusk, night).changing);
            assert(sample(dusk + 1.0 - 1.e-6, dawn, dusk, night).changing);
            assert(!sample(dusk + 1.0, dawn, dusk, night).changing);
            assert(std::abs(sample(dawn, dawn, dusk, night).tint[3] - night[3]) < 1.e-6f);
            assert(std::abs(sample(dusk, dawn, dusk, night).tint[3]) < 1.e-6f);
            assert(std::abs(sample(dawn + 1.0, dawn, dusk, night).tint[3]) < 1.e-6f);
            assert(std::abs(sample(dusk + 1.0, dawn, dusk, night).tint[3] - night[3]) < 1.e-6f);
            float lastDawn = 1.f, lastDusk = 0.f;
            for (int i = 0; i <= 3600; ++i) {
                const double offset = static_cast<double>(i) / 3600.0;
                const auto a = sample(dawn + offset, dawn, dusk, night).tint;
                const auto b = sample(dusk + offset, dawn, dusk, night).tint;
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
                                    dawn,
                                    dawn + .2,
                                    dawn + .4,
                                    dawn + .6,
                                    dawn + .8,
                                    dawn + 1.0,
                                    dusk - 1.0,
                                    dusk,
                                    dusk + .2,
                                    dusk + .4,
                                    dusk + .6,
                                    dusk + .8,
                                    dusk + 1.0,
                                    24.0}) {
                const auto left = sample(hour - 1.e-6, dawn, dusk, night).tint;
                const auto right = sample(hour + 1.e-6, dawn, dusk, night).tint;
                assert(std::abs(left[3] - right[3]) < 1.e-5f);
                for (std::size_t c = 0; c < 3; ++c)
                    assert(std::abs(left[c] * left[3] - right[c] * right[3]) < 1.e-5f);
            }
        }
    assert(sample(6.0, 6.0, 18.0, night).tint[3] > sample(6.5, 6.0, 18.0, night).tint[3]);
    assert(sample(6.5, 6.0, 18.0, night).tint[3] > 0.f);
    assert(sample(18.5, 6.0, 18.0, night).tint[3] < night[3]);
    // Every palette stop must be reached inside the same 60-minute warning,
    // including the gold/violet stops close to its end.
    const auto paleGold = sample(6.8, 6.0, 18.0, night).tint;
    assert(paleGold[0] > paleGold[1] && paleGold[1] > paleGold[2]);
    const auto lateViolet = sample(18.8, 6.0, 18.0, night).tint;
    assert(lateViolet[2] > lateViolet[0] && lateViolet[0] > lateViolet[1]);
    // Midnight can also be the start of a valid one-hour morning warning.
    assert(sample(0.5, 0.0, 18.0, night).changing);
    assert(!sample(23.99, 0.0, 18.0, night).changing);
    // Latest possible native dusk hour reaches full night at midnight.
    assert(sample(23.5, 6.0, 23.0, night).changing);
    assert(std::abs(sample(0.0, 6.0, 23.0, night).tint[3] - night[3]) < 1.e-6f);
    assert(!sample(5.0, 10.0, 6.0, night).changing);
    for (int i = 0; i < 1440; ++i)
        assert(std::abs(sample(i / 60.0, 6.0, 18.0, {0.f, 0.f, 0.f, 0.f}).tint[3]) < 1.e-6f);
    for (int i = 0; i <= 3600; ++i) {
        const double phase = static_cast<double>(i) / 3600.0;
        for (const double start : {6.0, 18.0}) {
            const auto current = sample(start + phase, 6.0, 18.0, night);
            const auto next = sample(start + phase + 1.e-6, 6.0, 18.0, night);
            for (std::size_t c = 0; c < 4; ++c) {
                assert(std::isfinite(current.highlights[c]));
                assert(current.highlights[c] >= 0.f && current.highlights[c] <= 1.f);
            }
            assert(std::abs(current.highlights[3] - next.highlights[3]) < 1.e-4f);
            for (std::size_t c = 0; c < 3; ++c)
                assert(std::abs(current.highlights[c] * current.highlights[3]
                                - next.highlights[c] * next.highlights[3])
                       < 1.e-4f);
        }
    }
    for (const double hour : {0.0, 5.0, 6.0, 7.0, 12.0, 17.0, 18.0, 19.0, 24.0})
        assert(std::abs(sample(hour, 6.0, 18.0, night).highlights[3]) < 1.e-6f);
    assert(sample(18.5, 6.0, 18.0, night).highlights[3] > 0.79f);
    const auto gold = sample(18.2, 6.0, 18.0, night).highlights;
    assert(gold[0] > gold[1] && gold[1] > gold[2]);
    const auto violet = sample(6.2, 6.0, 18.0, night).highlights;
    assert(violet[2] > violet[0] && violet[2] > violet[1]);
    for (int i = 0; i < 1440; ++i)
        assert(std::abs(sample(i / 60.0, 6.0, 18.0, {0.f, 0.f, 0.f, 0.f}).highlights[3]) < 1.e-6f);
    assert(!sample(5.0, 10.0, 6.0, night).changing);
    std::cout << "One-hour twilight colours, endpoints, highlight grade and continuous wrap passed\n";
}
