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
    if (visibility.enabled != m_selectionVisibility.enabled
        || visibility.known.generation != m_selectionVisibility.known.generation) {
        if (visibility.enabled != m_selectionVisibility.enabled) {
            m_fogGameplayZoom.reset();
        } else if (!m_fogAwaitingPlayer) {
            // During an interrupted arrival, the raw zoom still belongs to
            // the opening view; keep the animation's gameplay destination.
            m_fogGameplayZoom = m_fogCamera ? m_fogCamera->toScale : getRawZoom();
        }
        m_fogAwaitingPlayer = true;
        m_fogPlayerLocated = false;
        m_fogCamera.reset();
        m_fogOverviewBounds.reset();
        setTemporaryScale(std::nullopt);
    }
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
    m_frameManager.registerCallback(m_lifetime, [this]() {
        return m_fogCamera ? FrameManager::AnimationStatusEnum::Continue
                           : FrameManager::AnimationStatusEnum::Stop;
    });
}

void MapCanvas::updateFogOfWarView()
{
    const auto visibility = currentRoomVisibility();
    if (!visibility.enabled) {
        return;
    }
    if (!m_fogGameplayZoom) {
        // Capture before browsing the opening view can change the raw zoom.
        m_fogGameplayZoom = getRawZoom();
    }
    if (m_fogAwaitingPlayer && getConfig().canvas.backgroundMapEnabled.get()) {
        // Surface terrain supplies the opening view even if a previous session
        // ended underground. Bounds are cached by the independent loader worker.
        const auto bounds = m_backgroundMap.bounds(0);
        const auto size = getViewport().size;
        if (bounds && (bounds != m_fogOverviewBounds || size != m_fogOverviewSize)) {
            const auto scale = ProjectionUtils::calculateCoverScale(getViewportConfig(),
                                                                    bounds->minimum,
                                                                    bounds->maximum,
                                                                    size,
                                                                    0);
            if (scale) {
                stopFlick();
                m_fogOverviewBounds = bounds;
                m_fogOverviewSize = size;
                setCurrentLayer(0);
                const auto center = (bounds->minimum + bounds->maximum) * 0.5f;
                setScroll(center);
                setTemporaryScale(*scale);
                emit sig_onCenter(center);
            }
        }
    }
    const auto room = m_data.getCurrentRoom();
    if (m_fogAwaitingPlayer && m_fogPlayerLocated && room && width() > 0 && height() > 0
        && visibility.known.complete && visibility.allows(room) && m_batches.mapBatches
        && !m_batches.isInProgress() && !m_data.getNeedsMapUpdate()
        && m_batches.displayedVisibility.canDisplayFor(visibility)
        && m_batches.displayedVisibility.known.isKnown(room.getServerId())
        && (!getConfig().canvas.backgroundMapEnabled.get() || !m_backgroundMap.isLoading())) {
        m_fogAwaitingPlayer = false;
        const auto destination = room.getPosition();
        // A missing/disabled background supplies no opening view to animate from.
        if (m_fogOverviewBounds) {
            m_fogCamera = FogOfWarCamera{getScroll(),
                                         destination.to_vec2() + glm::vec2{0.5f},
                                         getTotalScaleFactor(),
                                         *m_fogGameplayZoom};
            m_fogCameraClock.start();
            m_frameManager.requestUpdate();
        } else {
            setZoom(*m_fogGameplayZoom);
            onMovement();
        }
    }
    if (m_fogCamera) {
        constexpr float DURATION_MS = 1400.f;
        const float fraction = static_cast<float>(m_fogCameraClock.elapsed()) / DURATION_MS;
        const auto pose = m_fogCamera->sample(fraction);
        setScroll(pose.center);
        setTemporaryScale(pose.scale);
        emit sig_onCenter(pose.center);
        if (fraction >= 1.f) {
            // Commit the gameplay zoom before removing the presentation scale.
            // Otherwise a manual overview zoom would reappear after arrival.
            setZoom(m_fogCamera->toScale);
            m_fogCamera.reset();
            onMovement();
        }
        return;
    }
}
