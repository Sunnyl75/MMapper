#pragma once
// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 The MMapper Authors

#include <array>
#include <cstdint>

class GmcpMessage;

namespace terrain_doors {
bool isExitUpdate(const GmcpMessage &message);
enum class State : uint8_t { UNKNOWN, CLOSED, OPEN };

// Only the current server-observed room is retained. This never changes the
// saved map, drives a timer, or requests terrain geometry to be regenerated.
class Observation final
{
private:
    uint32_t m_serverRoom = 0;
    bool m_known = false;
    std::array<State, 4> m_states{}; // N, S, E, W, matching ExitDirEnum.

public:
    bool observe(const GmcpMessage &message);
    bool clear();
    bool appliesTo(uint32_t currentServerRoom) const;
    unsigned openMask() const;
    State state(unsigned direction) const { return m_states.at(direction); }
    friend bool operator==(const Observation &, const Observation &) = default;
};
} // namespace terrain_doors
