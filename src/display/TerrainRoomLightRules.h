#pragma once
// SPDX-License-Identifier: GPL-2.0-or-later

#include "../map/mmapper2room.h"

#include <array>
#include <cstdint>
#include <optional>
#include <string_view>

#include <QJsonObject>

namespace terrain_room_light {

enum class Evidence { NONE, ENCLOSED_LIT, DESCRIPTION };
Evidence classify(RoomTerrainEnum terrain, RoomLightEnum light, std::string_view description);
std::array<uint8_t, 4> variation(uint32_t savedRoomId);

// Session observations never become permanent room classifications.
struct Observation final
{
    uint32_t serverRoom = 0;
    bool roomKnown = false;
    bool artificial = false;
    std::optional<bool> pending;

    bool operator==(const Observation &) const = default;
    void clear();
    void room(const QJsonObject &object);
    void vitals(const QJsonObject &object);
    bool appliesTo(uint32_t currentServerRoom) const;
};
} // namespace terrain_room_light
