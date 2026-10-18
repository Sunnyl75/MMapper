#pragma once
// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 The MMapper Authors

#include <array>
#include <cstddef>
#include <cstdint>

// Pure layout generation: no graphics context, global RNG or saved map data.
namespace terrain_sprites {
inline constexpr uint32_t NUM_VARIANTS = 6;
inline constexpr std::size_t SPRITES_PER_ROOM = 9;
inline constexpr float ZOOM_CUTOFF = 0.4f;
enum class Kind : int { TREE = 0, FIELD_DETAIL = 1, GROUND = 2, BASE = 3 };
constexpr int flags(const Kind kind, const uint32_t quarterTurns = 0)
{
    return static_cast<int>(kind) | (static_cast<int>(quarterTurns & 3u) << 4);
}

struct Sprite final
{
    float x;
    float y;
    float size;
    uint32_t variant;
    uint32_t transition = 0; // Stable per-tree timing, independent of the chosen artwork.
};

// Directions come from the same road flags used to select the visible trail.
// Each active arm joins the room centre to the midpoint of one room edge.
struct TrailSegments final
{
    bool north = false;
    bool south = false;
    bool east = false;
    bool west = false;
};

inline constexpr float TRAIL_HALF_WIDTH = 0.16f;
inline constexpr float TREE_BASE_CLEARANCE = 0.035f;

namespace detail {
constexpr bool nearTrailSegment(const float x, const float y, const float endX, const float endY)
{
    const float dx = endX - 0.5f;
    const float dy = endY - 0.5f;
    const float projection = ((x - 0.5f) * dx + (y - 0.5f) * dy) / (dx * dx + dy * dy);
    const float t = projection < 0.f ? 0.f : projection > 1.f ? 1.f : projection;
    const float distanceX = x - (0.5f + t * dx);
    const float distanceY = y - (0.5f + t * dy);
    constexpr float clearance = TRAIL_HALF_WIDTH + TREE_BASE_CLEARANCE;
    return distanceX * distanceX + distanceY * distanceY <= clearance * clearance;
}
} // namespace detail

constexpr bool blocksTree(const Sprite &sprite, const TrailSegments &trail)
{
    return (trail.north && detail::nearTrailSegment(sprite.x, sprite.y, 0.5f, 1.f))
           || (trail.south && detail::nearTrailSegment(sprite.x, sprite.y, 0.5f, 0.f))
           || (trail.east && detail::nearTrailSegment(sprite.x, sprite.y, 1.f, 0.5f))
           || (trail.west && detail::nearTrailSegment(sprite.x, sprite.y, 0.f, 0.5f));
}

// Fixed unsigned arithmetic gives identical layouts on every supported platform.
constexpr uint32_t mix(uint32_t x)
{
    x ^= x >> 16;
    x *= 0x7feb352du;
    x ^= x >> 15;
    x *= 0x846ca68bu;
    return x ^ (x >> 16);
}

constexpr std::array<Sprite, SPRITES_PER_ROOM> forest(const uint32_t roomId)
{
    std::array<Sprite, SPRITES_PER_ROOM> result{};
    for (uint32_t i = 0; i < SPRITES_PER_ROOM; ++i) {
        const uint32_t seed = mix(roomId ^ mix(i + 0x9e3779b9u));
        uint32_t h = seed;
        float x = 0.5f;
        float y = 0.5f;
        // Rejection sampling scatters stable anchors through the room while
        // avoiding clumps. Every room uses its saved ID as its reproducible seed.
        for (uint32_t attempt = 0; attempt < 48; ++attempt) {
            h = mix(seed ^ mix(attempt + 0x85ebca6bu));
            x = 0.035f + 0.93f * static_cast<float>(h & 65535u) / 65535.f;
            y = 0.035f + 0.93f * static_cast<float>(h >> 16) / 65535.f;
            bool spaced = true;
            for (uint32_t previous = 0; previous < i; ++previous) {
                const float dx = x - result[previous].x;
                const float dy = y - result[previous].y;
                if (dx * dx + dy * dy < 0.0144f) {
                    spaced = false;
                    break;
                }
            }
            if (spaced)
                break;
        }
        const float scale = static_cast<float>((h >> 8) & 255u) / 256.f;
        result[i] = Sprite{x,
                           y,
                           0.52f + 0.12f * scale,
                           mix(h) % NUM_VARIANTS,
                           mix(h ^ 0x73a4b91du) & 65535u};
    }
    return result;
}
// Ground and field details use independent, repeatable saved-room hash streams.
constexpr std::array<Sprite, SPRITES_PER_ROOM> ground(const uint32_t roomId)
{
    auto sprites = forest(roomId ^ 0xd37a9b45u);
    for (uint32_t i = 0; i < SPRITES_PER_ROOM; ++i) {
        auto &sprite = sprites[i];
        const uint32_t h = mix(roomId ^ mix(i + 0x47524f55u));
        sprite.y = (static_cast<float>(i / 3) + 0.25f
                    + 0.5f * static_cast<float>((h >> 8) & 255u) / 256.f)
                   / 3.f;
        sprite.size = 0.95f + 0.18f * static_cast<float>(h & 255u) / 256.f;
    }
    return sprites;
}
constexpr std::array<Sprite, SPRITES_PER_ROOM> field(const uint32_t roomId)
{
    auto sprites = forest(roomId ^ 0x4649454cu);
    for (auto &sprite : sprites)
        sprite.size = 0.23f + (sprite.size - 0.52f) * 0.7f;
    return sprites;
}
} // namespace terrain_sprites
