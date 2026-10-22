// SPDX-License-Identifier: GPL-2.0-or-later
#include "TerrainLightingDiagnostics.h"

#include <algorithm>
#include <utility>

#include <QDebug>
#include <QString>
#include <QtGlobal>

TerrainLightingDiagnostics::TerrainLightingDiagnostics()
    : m_logEnabled(qEnvironmentVariableIntValue("MMAPPER_TERRAIN_TIMINGS") > 0)
{}

TerrainLightingDiagnostics::Scope::Scope(TerrainLightingDiagnostics &owner, const Stage stage)
    : m_owner(owner.enabled() ? &owner : nullptr)
    , m_stage(stage)
    , m_start(m_owner ? Clock::now() : Clock::time_point{})
{
    if (m_owner && m_stage == Stage::FRAME)
        m_owner->startFrame(m_start);
}

TerrainLightingDiagnostics::Scope::~Scope()
{
    if (!m_owner)
        return;
    m_owner->record(m_stage, m_start);
    if (m_stage == Stage::FRAME)
        m_owner->finishFrame();
}

void TerrainLightingDiagnostics::record(const Stage stage, const Clock::time_point start)
{
    record(stage, start, Clock::now());
}

void TerrainLightingDiagnostics::record(const Stage stage,
                                        const Clock::time_point start,
                                        const Clock::time_point end)
{
    const double ms = std::chrono::duration<double, std::milli>(end - start).count();
    add(m_samples[static_cast<size_t>(stage)], ms);
    if (m_displayEnabled)
        add(m_displaySamples[static_cast<size_t>(stage)], ms);
    if (stage == Stage::DOOR_ROOM_INFO || stage == Stage::DOOR_EXIT_UPDATE
        || stage == Stage::DOOR_OTHER) {
        auto *trace = m_pendingTrace ? &*m_pendingTrace : m_activeTrace ? &*m_activeTrace : nullptr;
        if (trace)
            trace->doorCpuMs += ms;
    }
    if (stage == Stage::DOOR_SELECT)
        m_frameDoorSelectMs += ms;
    if (stage == Stage::DOOR_DRAW_SUBMIT)
        m_frameDoorDrawMs += ms;
}

void TerrainLightingDiagnostics::add(Sample &sample, const double ms)
{
    ++sample.count;
    sample.total += ms;
    sample.maximum = std::max(sample.maximum, ms);
}

void TerrainLightingDiagnostics::setLiveDoorsEnabled(const bool liveDoorsEnabled)
{
    m_liveDoorsEnabled = liveDoorsEnabled;
}

void TerrainLightingDiagnostics::roomContext(const uint32_t savedRoomId,
                                             const unsigned horizontalDoors,
                                             const unsigned verticalDoors)
{
    if (!enabled())
        return;
    m_roomContext = RoomContext{savedRoomId,
                                std::min(horizontalDoors, 4u),
                                std::min(verticalDoors, 2u)};
}

void TerrainLightingDiagnostics::setDisplayEnabled(const bool displayEnabled)
{
    if (m_displayEnabled == displayEnabled)
        return;
    m_displayEnabled = displayEnabled;
    m_displaySamples = {};
    m_displayRoomSamples = {};
    m_displayDoorRequests = 0;
    m_lastDoorWaitMs = -1.0;
    m_lastDoorSubmitMs = -1.0;
    m_lastMovementSubmitMs = -1.0;
    m_lastArrivalDoorMs = -1.0;
    if (!enabled()) {
        m_pendingDoorRequest.reset();
        m_frameDoorRequest.reset();
        m_pendingTrace.reset();
        m_activeTrace.reset();
        m_movement.reset();
        m_recenter.reset();
    }
}

QStringList TerrainLightingDiagnostics::performanceLines(const bool liveDoorsEnabled) const
{
    const auto pair = [this](const Stage stage) {
        const auto &sample = m_displaySamples[static_cast<size_t>(stage)];
        if (sample.count == 0)
            return QStringLiteral("n/a");
        return QStringLiteral("%1/%2")
            .arg(sample.total / static_cast<double>(sample.count), 0, 'f', 3)
            .arg(sample.maximum, 0, 'f', 3);
    };
    const auto last = [](const double ms) {
        return ms < 0.0 ? QStringLiteral("n/a") : QString::number(ms, 'f', 3);
    };
    QStringList lines{
        QStringLiteral("Live doors: %1 | redraw requests: %2 | CPU avg/max ms since stats enabled")
            .arg(liveDoorsEnabled ? QStringLiteral("ON") : QStringLiteral("OFF (test)"))
            .arg(qulonglong(m_displayDoorRequests)),
        QStringLiteral("Door CPU: room %1 | exits %2 | other/reset %3")
            .arg(pair(Stage::DOOR_ROOM_INFO),
                 pair(Stage::DOOR_EXIT_UPDATE),
                 pair(Stage::DOOR_OTHER)),
        QStringLiteral("Door CPU: frame selection %1 | draw submit %2 (all door sprites)")
            .arg(pair(Stage::DOOR_SELECT), pair(Stage::DOOR_DRAW_SUBMIT)),
        QStringLiteral("Door redraw: queue last %1 ms | request-to-submit last %2 ms (shared frame)")
            .arg(last(m_lastDoorWaitMs), last(m_lastDoorSubmitMs)),
        QStringLiteral("Movement: received GMCP-to-submit last %1 ms; network time excluded")
            .arg(last(m_lastMovementSubmitMs)),
        QStringLiteral("Last arrival door observer CPU: %1 ms").arg(last(m_lastArrivalDoorMs)),
        QStringLiteral(
            "Submission timings precede the existing stats GPU wait; they are not display latency.")};
    if (m_roomContext) {
        const auto &room = *m_roomContext;
        lines.prepend(QStringLiteral("Current map room %1: %2 doors (%3 horizontal, %4 up/down)")
                          .arg(room.mapRoom)
                          .arg(room.horizontal + room.vertical)
                          .arg(room.horizontal)
                          .arg(room.vertical));
    } else {
        lines.prepend(QStringLiteral("Current room door count: n/a"));
    }
    for (size_t i = 0; i < m_displayRoomSamples.size(); ++i) {
        const auto &sample = m_displayRoomSamples[i].movement;
        if (sample.count == 0)
            continue;
        const auto &observer = m_displayRoomSamples[i].observer;
        lines.append(QStringLiteral("Movement, %1 doors: n=%2 avg/max %3/%4 ms; door CPU avg %5 ms")
                         .arg(qulonglong(i))
                         .arg(qulonglong(sample.count))
                         .arg(sample.total / static_cast<double>(sample.count), 0, 'f', 3)
                         .arg(sample.maximum, 0, 'f', 3)
                         .arg(observer.total / static_cast<double>(observer.count), 0, 'f', 3));
    }
    return lines;
}

void TerrainLightingDiagnostics::settings(const bool lightingEnabled,
                                          const int strength,
                                          const int flicker,
                                          const bool blockingPerfDisplay,
                                          const int maximumFps)
{
    if (!enabled())
        return;
    const std::array<int, 4> next{lightingEnabled ? 1 : 0,
                                  strength,
                                  flicker,
                                  blockingPerfDisplay ? 1 : 0};
    if (next == m_settings && maximumFps == m_maximumFps)
        return;
    report(); // Do not mix measurements across settings changes.
    m_settings = next;
    m_maximumFps = maximumFps;
    if (m_logEnabled)
        qInfo().noquote()
            << QStringLiteral(
                   "[terrain-perf] lights=%1 strength=%2 flicker=%3 "
                   "blocking-perf-display=%4 max-fps=%5; CPU wall timings, NOT GPU or network latency")
                   .arg(next[0])
                   .arg(strength)
                   .arg(flicker)
                   .arg(next[3])
                   .arg(maximumFps);
}

void TerrainLightingDiagnostics::movement()
{
    if (!enabled())
        return;
    if (m_movement || m_pendingTrace)
        ++m_coalesced;
    m_movement = Clock::now(); // Latest EventMoved received by our observer.
    m_pendingTrace = MovementTrace{};
    m_pendingTrace->sequence = ++m_sequence;
    m_pendingTrace->received = *m_movement;
}

void TerrainLightingDiagnostics::redrawRequested(const Cause cause)
{
    if (!enabled())
        return;
    const auto now = Clock::now();
    ++m_requests[static_cast<size_t>(cause)];
    m_pendingCauses |= uint32_t{1} << static_cast<size_t>(cause);
    if (cause == Cause::DOOR) {
        if (!m_pendingDoorRequest)
            m_pendingDoorRequest = now;
        if (m_displayEnabled)
            ++m_displayDoorRequests;
    }
    if (cause == Cause::FLICKER) {
        ++m_flickerTicks;
        m_lastFlicker = now;
    }
    if (cause != Cause::MOVEMENT || !m_pendingTrace)
        return;
    auto &trace = *m_pendingTrace;
    if (!trace.firstRequest) {
        trace.firstRequest = now;
        trace.flickerAtRequest = m_flickerTicks;
        if (m_lastFlicker)
            trace.flickerAge = std::chrono::duration<double, std::milli>(now - *m_lastFlicker)
                                   .count();
        record(Stage::GMCP_TO_REQUEST, trace.received, now);
    }
    trace.latestRequest = now;
    ++trace.requests;
}

void TerrainLightingDiagnostics::paintAttempt(const bool accepted)
{
    if (!enabled())
        return;
    if (!accepted)
        ++m_rejectedPaints;
    if (m_pendingTrace && m_pendingTrace->firstRequest) {
        auto &trace = *m_pendingTrace;
        if (!trace.firstAttempt) {
            trace.firstAttempt = Clock::now();
            record(Stage::REQUEST_TO_ATTEMPT, *trace.firstRequest, *trace.firstAttempt);
        }
        if (!accepted)
            ++trace.rejected;
    }
}

void TerrainLightingDiagnostics::startFrame(const Clock::time_point now)
{
    m_frameStarted = now;
    m_frameDoorSelectMs = 0.0;
    m_frameDoorDrawMs = 0.0;
    m_frameSubmitted = false;
    m_frameDoorRequest = std::exchange(m_pendingDoorRequest, std::nullopt);
    if (m_frameDoorRequest) {
        record(Stage::DOOR_REQUEST_TO_START, *m_frameDoorRequest, now);
        m_lastDoorWaitMs = std::chrono::duration<double, std::milli>(now - *m_frameDoorRequest)
                               .count();
    }
    m_frameCauses = std::exchange(m_pendingCauses, 0);
    m_frameFlickerTicks = m_flickerTicks;
    // A GMCP-triggered paint before room matching must not consume this trace.
    if (m_pendingTrace && m_pendingTrace->firstRequest) {
        m_activeTrace = std::move(m_pendingTrace);
        m_pendingTrace.reset();
        record(Stage::REQUEST_TO_START, *m_activeTrace->firstRequest, now);
    }
}

void TerrainLightingDiagnostics::reportMovement(const Clock::time_point end)
{
    if (!m_activeTrace)
        return;
    const auto trace = std::move(*m_activeTrace);
    m_activeTrace.reset();
    record(Stage::MATCHED_GMCP_TO_SUBMIT, trace.received, end);
    const auto ms = [](const Clock::time_point a, const Clock::time_point b) {
        return std::chrono::duration<double, std::milli>(b - a).count();
    };
    if (m_logEnabled)
        qInfo().noquote()
            << QStringLiteral(
                   "[terrain-move] seq=%1 gmcp-to-request=%2ms request-to-attempt=%3ms "
                   "request-to-frame-start=%4ms latest-request-to-start=%5ms frame-cpu=%6ms "
                   "gmcp-to-submit=%7ms movement-requests=%8 rejected-paints=%9 "
                   "flicker-age-at-request=%10ms flicker-ticks-after-request=%11 "
                   "frame-requests[movement=%12 flicker=%13 observation=%14 settings=%15] max-fps=%16 door-request=%17")
                   .arg(qulonglong(trace.sequence))
                   .arg(ms(trace.received, *trace.firstRequest), 0, 'f', 3)
                   .arg(trace.firstAttempt ? ms(*trace.firstRequest, *trace.firstAttempt) : -1.0,
                        0,
                        'f',
                        3)
                   .arg(ms(*trace.firstRequest, m_frameStarted), 0, 'f', 3)
                   .arg(ms(*trace.latestRequest, m_frameStarted), 0, 'f', 3)
                   .arg(ms(m_frameStarted, end), 0, 'f', 3)
                   .arg(ms(trace.received, end), 0, 'f', 3)
                   .arg(qulonglong(trace.requests))
                   .arg(qulonglong(trace.rejected))
                   .arg(trace.flickerAge, 0, 'f', 3)
                   .arg(qulonglong(m_frameFlickerTicks - trace.flickerAtRequest))
                   .arg((m_frameCauses & 1u) != 0 ? 1 : 0)
                   .arg((m_frameCauses & 2u) != 0 ? 1 : 0)
                   .arg((m_frameCauses & 4u) != 0 ? 1 : 0)
                   .arg((m_frameCauses & 8u) != 0 ? 1 : 0)
                   .arg(m_maximumFps)
                   .arg((m_frameCauses & 16u) != 0 ? 1 : 0);
}

void TerrainLightingDiagnostics::drawSubmitted()
{
    if (!enabled() || m_frameSubmitted)
        return;
    m_frameSubmitted = true;
    const auto end = Clock::now();
    if (m_frameDoorRequest) {
        record(Stage::DOOR_REQUEST_TO_SUBMIT, *m_frameDoorRequest, end);
        m_lastDoorSubmitMs = std::chrono::duration<double, std::milli>(end - *m_frameDoorRequest)
                                 .count();
        m_frameDoorRequest.reset();
    }
    if (!m_activeTrace)
        return;
    record(Stage::MATCHED_GMCP_TO_DRAW_SUBMIT, m_activeTrace->received, end);
    m_lastMovementSubmitMs
        = std::chrono::duration<double, std::milli>(end - m_activeTrace->received).count();
    m_lastArrivalDoorMs = m_activeTrace->doorCpuMs;
    if (!m_roomContext)
        return;
    const auto &room = *m_roomContext;
    const auto count = static_cast<size_t>(room.horizontal + room.vertical);
    const auto addRoom = [this](RoomSamples &samples) {
        add(samples.movement, m_lastMovementSubmitMs);
        add(samples.observer, m_lastArrivalDoorMs);
        add(samples.selection, m_frameDoorSelectMs);
        add(samples.draw, m_frameDoorDrawMs);
    };
    addRoom(m_roomSamples[count]);
    if (m_displayEnabled)
        addRoom(m_displayRoomSamples[count]);
    if (m_logEnabled)
        qInfo().noquote()
            << QStringLiteral(
                   "[terrain-door-room] seq=%1 map-room=%2 doors=%3 horizontal=%4 up-down=%5 live-doors=%6 "
                   "arrival-door-observer=%7ms door-select=%8ms door-draw-submit=%9ms "
                   "received-gmcp-to-submit=%10ms; before stats GPU wait, network excluded")
                   .arg(qulonglong(m_activeTrace->sequence))
                   .arg(room.mapRoom)
                   .arg(room.horizontal + room.vertical)
                   .arg(room.horizontal)
                   .arg(room.vertical)
                   .arg(m_liveDoorsEnabled ? 1 : 0)
                   .arg(m_lastArrivalDoorMs, 0, 'f', 6)
                   .arg(m_frameDoorSelectMs, 0, 'f', 6)
                   .arg(m_frameDoorDrawMs, 0, 'f', 6)
                   .arg(m_lastMovementSubmitMs, 0, 'f', 3);
}

void TerrainLightingDiagnostics::recenter()
{
    if (enabled())
        m_recenter = Clock::now();
}

void TerrainLightingDiagnostics::active()
{
    if (enabled())
        ++m_activeFrames;
}

void TerrainLightingDiagnostics::cachedLights(const size_t count)
{
    if (enabled())
        m_cachedLights = count;
}

void TerrainLightingDiagnostics::uploaded(const int width, const int height)
{
    if (!enabled())
        return;
    m_width = width;
    m_height = height;
    m_uploadBytes += static_cast<uint64_t>(width) * static_cast<uint64_t>(height) * 4;
}

void TerrainLightingDiagnostics::finishFrame()
{
    reportMovement(Clock::now());
    if (m_movement) {
        record(Stage::GMCP_TO_FRAME, *m_movement);
        m_movement.reset();
    }
    if (m_recenter) {
        record(Stage::RECENTER_TO_FRAME, *m_recenter);
        m_recenter.reset();
    }
    if (Clock::now() - m_window >= std::chrono::seconds{3})
        report();
}

void TerrainLightingDiagnostics::report()
{
    const auto frames = m_samples[static_cast<size_t>(Stage::FRAME)].count;
    if (frames > 0 && m_logEnabled) {
        const double seconds = std::chrono::duration<double>(Clock::now() - m_window).count();
        qInfo().noquote() << QStringLiteral(
                                 "[terrain-perf] window=%1s frames=%2 active-light-frames=%3 "
                                 "cached-lights=%4 mask=%5x%6 uploaded=%7KiB coalesced-moves=%8")
                                 .arg(seconds, 0, 'f', 2)
                                 .arg(qulonglong(frames))
                                 .arg(qulonglong(m_activeFrames))
                                 .arg(qulonglong(m_cachedLights))
                                 .arg(m_width)
                                 .arg(m_height)
                                 .arg(double(m_uploadBytes) / 1024.0, 0, 'f', 1)
                                 .arg(qulonglong(m_coalesced));
        qInfo().noquote()
            << QStringLiteral(
                   "[terrain-perf] redraw-requests movement=%1 flicker=%2 observation=%3 settings=%4 rejected-paints=%5 doors=%6 live-doors=%7")
                   .arg(qulonglong(m_requests[0]))
                   .arg(qulonglong(m_requests[1]))
                   .arg(qulonglong(m_requests[2]))
                   .arg(qulonglong(m_requests[3]))
                   .arg(qulonglong(m_rejectedPaints))
                   .arg(qulonglong(m_requests[static_cast<size_t>(Cause::DOOR)]))
                   .arg(m_liveDoorsEnabled ? 1 : 0);
        constexpr std::array names{"frame-cpu",
                                   "pending-gl",
                                   "batches",
                                   "draw-submit",
                                   "light-prepare",
                                   "light-index",
                                   "light-current-room",
                                   "light-visibility",
                                   "light-visibility-rebuild",
                                   "mask-build",
                                   "texture-upload-submit",
                                   "gmcp-observe",
                                   "latest-gmcp-to-frame-submit",
                                   "recenter-to-frame-submit",
                                   "gmcp-to-movement-request",
                                   "movement-request-to-first-paint-attempt",
                                   "movement-request-to-frame-start",
                                   "matched-gmcp-to-frame-submit",
                                   "door-observe-room",
                                   "door-observe-exits",
                                   "door-observe-other-reset",
                                   "door-frame-selection",
                                   "door-draw-submit",
                                   "door-request-to-frame-start",
                                   "door-request-to-draw-submit",
                                   "matched-gmcp-to-draw-submit"};
        static_assert(names.size() == static_cast<size_t>(Stage::COUNT));
        for (size_t i = 0; i < names.size(); ++i) {
            const auto &sample = m_samples[i];
            if (sample.count == 0)
                continue;
            qInfo().noquote() << QStringLiteral(
                                     "[terrain-perf] %1 n=%2 avg=%3ms max=%4ms total=%5ms")
                                     .arg(QString::fromLatin1(names[i]))
                                     .arg(qulonglong(sample.count))
                                     .arg(sample.total / double(sample.count), 0, 'f', 6)
                                     .arg(sample.maximum, 0, 'f', 6)
                                     .arg(sample.total, 0, 'f', 3);
        }
        for (size_t i = 0; i < m_roomSamples.size(); ++i) {
            const auto &room = m_roomSamples[i];
            if (room.movement.count == 0)
                continue;
            qInfo().noquote()
                << QStringLiteral(
                       "[terrain-door-bucket] doors=%1 live-doors=%2 n=%3 "
                       "gmcp-to-submit-avg=%4ms max=%5ms arrival-door-cpu-avg=%6ms "
                       "select-avg=%7ms draw-submit-avg=%8ms; room count is context, not proof of causation")
                       .arg(qulonglong(i))
                       .arg(m_liveDoorsEnabled ? 1 : 0)
                       .arg(qulonglong(room.movement.count))
                       .arg(room.movement.total / static_cast<double>(room.movement.count),
                            0,
                            'f',
                            3)
                       .arg(room.movement.maximum, 0, 'f', 3)
                       .arg(room.observer.total / static_cast<double>(room.observer.count),
                            0,
                            'f',
                            6)
                       .arg(room.selection.total / static_cast<double>(room.selection.count),
                            0,
                            'f',
                            6)
                       .arg(room.draw.total / static_cast<double>(room.draw.count), 0, 'f', 6);
        }
    }
    m_samples = {};
    m_roomSamples = {};
    m_activeFrames = 0;
    m_uploadBytes = 0;
    m_coalesced = 0;
    m_requests = {};
    m_rejectedPaints = 0;
    m_window = Clock::now();
}
