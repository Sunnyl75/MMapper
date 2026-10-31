#pragma once
// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 The MMapper Authors

#include "../global/macros.h"

#include <algorithm>
#include <cmath>

#include <glm/glm.hpp>

// A transient view animation, independent of map data and discovery tracking.
struct NODISCARD FogOfWarCamera final
{
    glm::vec2 fromCenter{0.f};
    glm::vec2 toCenter{0.f};
    float fromScale = 1.f;
    float toScale = 1.f;

    struct NODISCARD Pose final
    {
        glm::vec2 center;
        float scale;
    };

    NODISCARD Pose sample(const float fraction) const
    {
        const float t = std::clamp(fraction, 0.f, 1.f);
        const float eased = t * t * (3.f - 2.f * t);
        return {glm::mix(fromCenter, toCenter, eased),
                std::exp(glm::mix(std::log(fromScale), std::log(toScale), eased))};
    }
};
