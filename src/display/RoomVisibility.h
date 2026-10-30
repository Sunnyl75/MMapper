#pragma once
// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 The MMapper Authors

#include "../map/Map.h"
#include "../map/RoomHandle.h"
#include "../observer/KnownRooms.h"

// An immutable render policy, not a second discovery model or map.
struct NODISCARD RoomVisibility final
{
    bool enabled = false;
    KnownRooms::Snapshot known;

    NODISCARD bool hidesAllRooms() const
    {
        return enabled && (!known.complete || known.rooms.empty());
    }

    NODISCARD bool allows(const RoomHandle &room) const
    {
        return !enabled || known.isKnown(room.getServerId());
    }

    NODISCARD bool matches(const RoomVisibility &other) const
    {
        return enabled == other.enabled
               && (!enabled
                   || (known.revision == other.known.revision
                       && known.generation == other.known.generation
                       && known.complete == other.known.complete));
    }

    // Previously rendered rooms remain safe during additions. A replacement,
    // character switch, or enabling fog must hide incompatible meshes at once.
    NODISCARD bool canDisplayFor(const RoomVisibility &desired) const
    {
        return !desired.enabled
               || (enabled && known.generation == desired.known.generation
                   && known.epoch == desired.known.epoch
                   && known.complete == desired.known.complete);
    }
};
