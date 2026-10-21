#pragma once
// SPDX-License-Identifier: GPL-2.0-or-later
#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <optional>

// Opt-in, main-thread CPU wall timings. Never synchronizes with the GPU.
class TerrainLightingDiagnostics final
{
public:
    enum class Stage : size_t {
        FRAME,
        PENDING_GL,
        BATCHES,
        DRAW,
        PREPARE,
        INDEX,
        CURRENT_ROOM_INDEX,
        VISIBILITY,
        VISIBILITY_REBUILD,
        MASK,
        UPLOAD,
        OBSERVATION,
        GMCP_TO_FRAME,
        RECENTER_TO_FRAME,
        GMCP_TO_REQUEST,
        REQUEST_TO_ATTEMPT,
        REQUEST_TO_START,
        MATCHED_GMCP_TO_SUBMIT,
        COUNT
    };
    enum class Cause : size_t { MOVEMENT, FLICKER, OBSERVATION, SETTINGS, COUNT };
    using Clock = std::chrono::steady_clock;
    class Scope final
    {
        TerrainLightingDiagnostics *m_owner;
        Stage m_stage;
        Clock::time_point m_start;

    public:
        Scope(TerrainLightingDiagnostics &owner, Stage stage);
        Scope(const Scope &) = delete;
        Scope &operator=(const Scope &) = delete;
        ~Scope();
    };

private:
    struct Sample final
    {
        uint64_t count = 0;
        double total = 0.0;
        double maximum = 0.0;
    };
    struct MovementTrace final
    {
        uint64_t sequence = 0;
        Clock::time_point received;
        std::optional<Clock::time_point> firstRequest;
        std::optional<Clock::time_point> latestRequest;
        std::optional<Clock::time_point> firstAttempt;
        uint64_t flickerAtRequest = 0;
        uint64_t requests = 0;
        uint64_t rejected = 0;
        double flickerAge = -1.0;
    };
    const bool m_enabled;
    std::optional<MovementTrace> m_pendingTrace;
    std::optional<MovementTrace> m_activeTrace;
    std::optional<Clock::time_point> m_lastFlicker;
    Clock::time_point m_frameStarted;
    std::array<uint64_t, static_cast<size_t>(Cause::COUNT)> m_requests{};
    uint64_t m_sequence = 0;
    uint64_t m_flickerTicks = 0;
    uint64_t m_frameFlickerTicks = 0;
    uint64_t m_rejectedPaints = 0;
    uint32_t m_pendingCauses = 0;
    uint32_t m_frameCauses = 0;
    int m_maximumFps = -1;
    std::array<Sample, static_cast<size_t>(Stage::COUNT)> m_samples{};
    Clock::time_point m_window = Clock::now();
    std::optional<Clock::time_point> m_movement;
    std::optional<Clock::time_point> m_recenter;
    std::array<int, 4> m_settings{-1, -1, -1, -1};
    uint64_t m_coalesced = 0;
    uint64_t m_uploadBytes = 0;
    uint64_t m_activeFrames = 0;
    size_t m_cachedLights = 0;
    int m_width = 0;
    int m_height = 0;

    void record(Stage stage, Clock::time_point start);
    void record(Stage stage, Clock::time_point start, Clock::time_point end);
    void startFrame(Clock::time_point now);
    void reportMovement(Clock::time_point end);
    void finishFrame();
    void report();

public:
    TerrainLightingDiagnostics();
    Scope measure(Stage stage) { return Scope{*this, stage}; }
    void settings(bool enabled, int strength, int flicker, bool blockingPerfDisplay, int maximumFps);
    void redrawRequested(Cause cause);
    void paintAttempt(bool accepted);
    void movement();
    void recenter();
    void active();
    void cachedLights(size_t count);
    void uploaded(int width, int height);
};
