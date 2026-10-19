// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 The MMapper Authors
#include "TerrainDoors.h"

#include <limits>

#include <QJsonArray>
#include <QJsonObject>
#include <QJsonValue>

namespace terrain_doors {
bool Observation::observe(const QJsonObject &object, const bool partial)
{
    auto next = partial ? *this : Observation{};
    if (object.contains(QStringLiteral("id"))) {
        const auto id = object.value(QStringLiteral("id")).toInteger(-1);
        if (id <= 0 || id > std::numeric_limits<uint32_t>::max()) {
            if (partial)
                return false;
            next.clear();
        } else {
            const auto room = static_cast<uint32_t>(id);
            if (partial && room != serverRoom)
                return false;
            next.serverRoom = room;
        }
    }
    if (next.serverRoom != 0) {
        const auto value = object.value(QStringLiteral("exits"));
        // Room.Info has a complete exit snapshot. Partial updates must include
        // an explicit exit object; absent flags are not evidence of opening.
        const auto exits = value.isObject() ? value.toObject() : partial ? object : QJsonObject{};
        const std::array<QString, 4> directions{QStringLiteral("n"),
                                                QStringLiteral("s"),
                                                QStringLiteral("e"),
                                                QStringLiteral("w")};
        for (std::size_t i = 0; i < directions.size(); ++i) {
            if (!exits.contains(directions[i]))
                continue;
            const auto entry = exits.value(directions[i]);
            if (entry.isNull()) {
                next.states[i] = State::UNKNOWN;
                continue;
            }
            if (!entry.isObject())
                continue;
            const auto exit = entry.toObject();
            const auto flags = exit.value(QStringLiteral("flags"));
            if (partial && !flags.isArray())
                continue;
            const auto list = flags.toArray();
            next.states[i] = list.contains(QJsonValue{QStringLiteral("closed")}) ? State::CLOSED
                                                                                 : State::OPEN;
        }
    }
    if (next == *this)
        return false;
    *this = next;
    return true;
}
} // namespace terrain_doors
