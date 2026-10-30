// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 The MMapper Authors

#include "../configuration/configuration.h"
#include "../observer/gameobserver.h"
#include "mapcanvas.h"

RoomVisibility MapCanvas::currentRoomVisibility() const
{
    return RoomVisibility{getConfig().canvas.fogOfWarEnabled.get(), m_observer.getKnownRooms()};
}

RoomVisibility MapCanvas::displayedRoomVisibility() const
{
    const auto desired = currentRoomVisibility();
    if (m_batches.mapBatches && m_batches.displayedVisibility.canDisplayFor(desired)) {
        return m_batches.displayedVisibility;
    }
    // No foreground is currently drawable, so it must cast no shadow either.
    return RoomVisibility{true, {}};
}

void MapCanvas::fogOfWarChanged()
{
    const auto visibility = currentRoomVisibility();
    if (!m_selectionVisibility.canDisplayFor(visibility)) {
        slot_clearAllSelections();
    }
    m_selectionVisibility = visibility;
    m_frameManager.requestUpdate();
}

void MapCanvas::initFogOfWar()
{
    m_observer.sig2_knownRoomsChanged.connect(m_lifetime, [this]() { fogOfWarChanged(); });
    setConfig().canvas.fogOfWarEnabled.registerChangeCallback(m_lifetime,
                                                              [this]() { fogOfWarChanged(); });
    fogOfWarChanged();
}
