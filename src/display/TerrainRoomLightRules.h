#pragma once
// SPDX-License-Identifier: GPL-2.0-or-later

#include "../clock/mumemoment.h"
#include "../map/mmapper2room.h"

#include <array>
#include <cstdint>
#include <string_view>

#include <QJsonObject>

namespace terrain_room_light {

enum class Evidence { NONE, DESCRIPTION, STREET_LAMP };
constexpr bool streetLampsOn(const MumeTimeEnum time)
{
    return time == MumeTimeEnum::DAWN || time == MumeTimeEnum::DUSK || time == MumeTimeEnum::NIGHT;
}
Evidence classify(RoomTerrainEnum terrain,
                  RoomLightEnum light,
                  std::string_view description,
                  std::string_view contents = {});
std::array<uint8_t, 4> variation(uint32_t savedRoomId);

// Session observations never become permanent room classifications.
struct Observation final
{
    uint32_t serverRoom = 0;
    bool roomKnown = false;
    bool artificial = false;

    bool operator==(const Observation &) const = default;
    void clear();
    void moved();
    void room(const QJsonObject &object);
    void vitals(const QJsonObject &object);
    bool appliesTo(uint32_t currentServerRoom) const;
};
} // namespace terrain_room_light
