// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 The MMapper Authors
#include "TerrainDoorState.h"

#include "../global/CaseUtils.h"
#include "../proxy/GmcpMessage.h"

#include <limits>

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

namespace terrain_doors {
bool isExitUpdate(const GmcpMessage &message)
{
    // Keep the existing core message table untouched. Its historical spelling
    // contains an extra dot; MUME's actual notification is Room.UpdateExits.
    return message.isRoomUpdateExits()
           || areEqualAsLowerUtf8(message.getName().getStdStringUtf8(), "room.updateexits");
}
namespace {
void updateStates(std::array<State, 4> &states, const QJsonObject &exits)
{
    constexpr std::array<const char *, 4> directions{"n", "s", "e", "w"};
    for (std::size_t i = 0; i < directions.size(); ++i) {
        const auto entry = exits.value(QLatin1String(directions[i]));
        if (entry.isBool() && !entry.toBool()) {
            states[i] = State::UNKNOWN; // The server removed this exit.
            continue;
        }
        if (!entry.isObject())
            continue; // Omitted directions retain their last observation.
        const auto flags = entry.toObject().value(QStringLiteral("flags"));
        if (!flags.isUndefined() && !flags.isArray())
            continue;
        // Each included exit is a replacement description of that exit, not
        // a patch to its previous flags. The open update omits flags entirely.
        states[i] = flags.toArray().contains(QStringLiteral("closed")) ? State::CLOSED
                                                                       : State::OPEN;
    }
}
} // namespace

bool Observation::clear()
{
    const bool changed = *this != Observation{};
    *this = Observation{};
    return changed;
}
bool Observation::observe(const GmcpMessage &message)
{
    if (message.isEventMoved() || message.isCoreGoodbye())
        return clear();
    const bool roomInfo = message.isRoomInfo();
    if ((!roomInfo && !isExitUpdate(message)) || !message.getJson())
        return false;
    const auto document = QJsonDocument::fromJson(message.getJson()->toQString().toUtf8());
    if (!document.isObject())
        return false;
    const auto object = document.object();
    auto next = *this;
    if (roomInfo) {
        next = Observation{};
        const auto id = object.value(QStringLiteral("id")).toDouble(0.0);
        if (id < 1.0 || id > static_cast<double>(std::numeric_limits<uint32_t>::max()))
            return clear(); // No reliable server identity: keep the decorative marker.
        next.m_serverRoom = static_cast<uint32_t>(id);
        if (static_cast<double>(next.m_serverRoom) < id)
            return clear();
        next.m_known = true;
        updateStates(next.m_states, object.value(QStringLiteral("exits")).toObject());
    } else {
        if (!m_known)
            return false; // An update cannot identify a room before Room.Info.
        updateStates(next.m_states, object);
    }
    if (next == *this)
        return false;
    *this = next;
    return true;
}
bool Observation::appliesTo(const uint32_t currentServerRoom) const
{
    // Path matching may still be showing the previous room when GMCP arrives.
    return m_known && m_serverRoom == currentServerRoom;
}
unsigned Observation::openMask() const
{
    unsigned result = 0;
    for (unsigned i = 0; i < m_states.size(); ++i)
        if (m_states[i] == State::OPEN)
            result |= 1u << i;
    return result;
}
} // namespace terrain_doors
