// SPDX-License-Identifier: GPL-2.0-or-later
#include "TerrainLightingDiagnostics.h"

#include <algorithm>
#include <utility>

#include <QDebug>
#include <QString>
#include <QtGlobal>

TerrainLightingDiagnostics::TerrainLightingDiagnostics()
    : m_enabled(qEnvironmentVariableIntValue("MMAPPER_TERRAIN_TIMINGS") > 0)
{}

TerrainLightingDiagnostics::Scope::Scope(TerrainLightingDiagnostics &owner, const Stage stage)
    : m_owner(owner.m_enabled ? &owner : nullptr)
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
    auto &sample = m_samples[static_cast<size_t>(stage)];
    const double ms = std::chrono::duration<double, std::milli>(end - start).count();
    ++sample.count;
    sample.total += ms;
    sample.maximum = std::max(sample.maximum, ms);
}

void TerrainLightingDiagnostics::settings(const bool enabled,
                                          const int strength,
                                          const int flicker,
                                          const bool blockingPerfDisplay,
                                          const int maximumFps)
{
    if (!m_enabled)
        return;
    const std::array<int, 4> next{enabled ? 1 : 0, strength, flicker, blockingPerfDisplay ? 1 : 0};
    if (next == m_settings && maximumFps == m_maximumFps)
        return;
    report(); // Do not mix measurements across settings changes.
    m_settings = next;
    m_maximumFps = maximumFps;
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
    if (!m_enabled)
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
    if (!m_enabled)
        return;
    const auto now = Clock::now();
    ++m_requests[static_cast<size_t>(cause)];
    m_pendingCauses |= uint32_t{1} << static_cast<size_t>(cause);
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
    if (!m_enabled)
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
    qInfo().noquote()
        << QStringLiteral(
               "[terrain-move] seq=%1 gmcp-to-request=%2ms request-to-attempt=%3ms "
               "request-to-frame-start=%4ms latest-request-to-start=%5ms frame-cpu=%6ms "
               "gmcp-to-submit=%7ms movement-requests=%8 rejected-paints=%9 "
               "flicker-age-at-request=%10ms flicker-ticks-after-request=%11 "
               "frame-requests[movement=%12 flicker=%13 observation=%14 settings=%15] max-fps=%16")
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
               .arg(m_maximumFps);
}

void TerrainLightingDiagnostics::recenter()
{
    if (m_enabled)
        m_recenter = Clock::now();
}

void TerrainLightingDiagnostics::active()
{
    if (m_enabled)
        ++m_activeFrames;
}

void TerrainLightingDiagnostics::cachedLights(const size_t count)
{
    if (m_enabled)
        m_cachedLights = count;
}

void TerrainLightingDiagnostics::uploaded(const int width, const int height)
{
    if (!m_enabled)
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
    if (frames > 0) {
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
                   "[terrain-perf] redraw-requests movement=%1 flicker=%2 observation=%3 settings=%4 rejected-paints=%5")
                   .arg(qulonglong(m_requests[0]))
                   .arg(qulonglong(m_requests[1]))
                   .arg(qulonglong(m_requests[2]))
                   .arg(qulonglong(m_requests[3]))
                   .arg(qulonglong(m_rejectedPaints));
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
                                   "matched-gmcp-to-frame-submit"};
        static_assert(names.size() == static_cast<size_t>(Stage::COUNT));
        for (size_t i = 0; i < names.size(); ++i) {
            const auto &sample = m_samples[i];
            if (sample.count == 0)
                continue;
            qInfo().noquote() << QStringLiteral(
                                     "[terrain-perf] %1 n=%2 avg=%3ms max=%4ms total=%5ms")
                                     .arg(QString::fromLatin1(names[i]))
                                     .arg(qulonglong(sample.count))
                                     .arg(sample.total / double(sample.count), 0, 'f', 3)
                                     .arg(sample.maximum, 0, 'f', 3)
                                     .arg(sample.total, 0, 'f', 3);
        }
    }
    m_samples = {};
    m_activeFrames = 0;
    m_uploadBytes = 0;
    m_coalesced = 0;
    m_requests = {};
    m_rejectedPaints = 0;
    m_window = Clock::now();
}
