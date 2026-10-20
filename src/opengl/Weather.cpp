// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 The MMapper Authors

#include "Weather.h"

#include "../clock/mumeclock.h"
#include "../configuration/NamedConfig.h"
#include "../configuration/configuration.h"
#include "../display/FrameManager.h"
#include "../display/TerrainTwilight.h"
#include "../display/Textures.h"
#include "../global/Badge.h"
#include "../map/coordinate.h"
#include "../mapdata/mapdata.h"
#include "./legacy/Legacy.h"
#include "OpenGL.h"
#include "OpenGLTypes.h"

#include <algorithm>
#include <cstdlib>
#include <utility>

#include <glm/glm.hpp>
#include <glm/gtx/compatibility.hpp>

/**
 * Weather transition authority documentation:
 *
 * Transitions (weather intensities, fog, time-of-day) are driven by a shared
 * duration constant (WeatherConstants::TRANSITION_DURATION).
 *
 * The CPU (GLWeather::update/applyTransition) calculates interpolated values
 * each frame. These CPU-side values are primarily used for high-level logic,
 * such as:
 * - Pruning: Skipping rendering of atmosphere/particles when intensities are zero.
 * - Thinning: Scaling the number of particle instances based on intensity for performance.
 * - Pacing: Determining if FrameManager heartbeat should continue.
 *
 * The GPU (shaders) is authoritative for weather and legacy time-of-day curves.
 * By passing start/target pairs and start times to the shaders, the GPU
 * performs per-pixel (or per-vertex) interpolation using its own clocks.
 * This ensures perfectly smooth visuals even if the CPU frame rate is low
 * or inconsistent, while avoiding constant UBO re-uploads for animation state.
 * Clock-driven dawn/dusk instead samples a continuous CPU palette and sends
 * one colour uniform to the existing time-of-day mesh. It does not rebuild
 * weather or terrain geometry.
 */

GLWeather::GLWeather(OpenGL &gl,
                     MapData &mapData,
                     const MapCanvasTextures &textures,
                     GameObserver &observer,
                     FrameManager &animationManager)
    : m_gl(gl)
    , m_data(mapData)
    , m_textures(textures)
    , m_animationManager(animationManager)
    , m_observer(observer)
{
    updateFromGame();

    m_moonVisibility = m_observer.getMoonVisibility();
    m_currentTimeOfDay = m_observer.getTimeOfDay();
    m_gameTimeOfDayIntensity = (m_currentTimeOfDay == MumeTimeEnum::DAY) ? 0.0f : 1.0f;

    {
        auto &wbInit = m_gl.getUboManager().get<Legacy::SharedVboEnum::WeatherBlock>();
        const NamedColorEnum targetColorIdx = getCurrentColorIdx();

        wbInit.timeOfDay.x = static_cast<float>(targetColorIdx); // startIdx
        wbInit.timeOfDay.y = static_cast<float>(targetColorIdx); // targetIdx
        wbInit.timeOfDay.z = m_gameTimeOfDayIntensity;           // todStart
        wbInit.timeOfDay.w = m_gameTimeOfDayIntensity;           // todTarget

        wbInit.intensities = glm::vec4(m_gameRainIntensity,
                                       m_gameSnowIntensity,
                                       m_gameCloudsIntensity,
                                       m_gameFogIntensity);

        updateTargets();
        wbInit.intensities = wbInit.targets;

        wbInit.config.x = -2.0f; // weatherStartTime
        wbInit.config.y = -2.0f; // todStartTime
        wbInit.config.z = WeatherConstants::TRANSITION_DURATION;
    }

    auto startWeatherTransitions = [this]() {
        auto &wb = m_gl.getUboManager().get<Legacy::SharedVboEnum::WeatherBlock>();

        const bool startingFromNice = (m_currentRainIntensity <= 0.0f
                                       && m_currentSnowIntensity <= 0.0f);

        startTransitions(wb.config.x,
                         TransitionPair<float>{wb.intensities.x, wb.targets.x},
                         TransitionPair<float>{wb.intensities.y, wb.targets.y},
                         TransitionPair<float>{wb.intensities.z, wb.targets.z},
                         TransitionPair<float>{wb.intensities.w, wb.targets.w});

        if (startingFromNice) {
            // If we are starting from clear skies, snap the start intensity for rain/snow
            // immediately so the ratio (pType) is correct during fade-in.
            wb.intensities.x = wb.targets.x > 0.0f ? 0.001f : 0.0f;
            wb.intensities.y = wb.targets.y > 0.0f ? 0.001f : 0.0f;
        }
    };

    m_observer.sig2_weatherChanged.connect(m_lifetime,
                                           [this, startWeatherTransitions](PromptWeatherEnum) {
                                               startWeatherTransitions();
                                               updateFromGame();
                                               updateTargets();
                                               syncWeatherAtmosphere();
                                               m_animationManager.requestUpdate();
                                           });

    m_observer.sig2_fogChanged.connect(m_lifetime, [this, startWeatherTransitions](PromptFogEnum) {
        startWeatherTransitions();
        updateFromGame();
        updateTargets();
        syncWeatherAtmosphere();
        m_animationManager.requestUpdate();
    });

    auto startTimeOfDayTransitions = [this]() {
        auto &wb = m_gl.getUboManager().get<Legacy::SharedVboEnum::WeatherBlock>();
        auto startColor = static_cast<NamedColorEnum>(static_cast<int>(wb.timeOfDay.x));
        auto targetColor = static_cast<NamedColorEnum>(static_cast<int>(wb.timeOfDay.y));

        startTransitions(wb.config.y,
                         TransitionPair<float>{wb.timeOfDay.z, wb.timeOfDay.w},
                         TransitionPair<NamedColorEnum>{startColor, targetColor});

        // Capture the snapped current color as the new transition start.
        // Callers that change the target color set wb.timeOfDay.y themselves.
        wb.timeOfDay.x = static_cast<float>(startColor);
    };

    m_observer.sig2_timeOfDayChanged
        .connect(m_lifetime, [this, startTimeOfDayTransitions](MumeTimeEnum timeOfDay) {
            if (timeOfDay == m_currentTimeOfDay) {
                return;
            }

            startTimeOfDayTransitions();

            m_currentTimeOfDay = timeOfDay;
            m_gameTimeOfDayIntensity = (timeOfDay == MumeTimeEnum::DAY) ? 0.0f : 1.0f;
            updateTargets();

            auto &wb_tod = m_gl.getUboManager().get<Legacy::SharedVboEnum::WeatherBlock>();
            wb_tod.timeOfDay.y = static_cast<float>(getCurrentColorIdx());

            syncWeatherTimeOfDay();
            m_animationManager.requestUpdate();
        });

    m_observer.sig2_moonVisibilityChanged
        .connect(m_lifetime, [this, startTimeOfDayTransitions](MumeMoonVisibilityEnum visibility) {
            if (visibility == m_moonVisibility) {
                return;
            }
            startTimeOfDayTransitions();

            m_moonVisibility = visibility;
            auto &wb_moon = m_gl.getUboManager().get<Legacy::SharedVboEnum::WeatherBlock>();
            wb_moon.timeOfDay.y = static_cast<float>(getCurrentColorIdx());

            syncWeatherTimeOfDay();
            m_animationManager.requestUpdate();
        });

    auto onSettingChanged = [this, startWeatherTransitions]() {
        startWeatherTransitions();
        updateTargets();
        syncWeatherAtmosphere();
        m_animationManager.requestUpdate();
    };

    auto onTimeOfDaySettingChanged = [this, startTimeOfDayTransitions]() {
        startTimeOfDayTransitions();
        updateTargets();
        syncWeatherTimeOfDay();
        m_animationManager.requestUpdate();
    };

    setConfig().canvas.weatherPrecipitationIntensity.registerChangeCallback(m_lifetime,
                                                                            onSettingChanged);
    setConfig().canvas.weatherAtmosphereIntensity.registerChangeCallback(m_lifetime,
                                                                         onSettingChanged);
    setConfig().canvas.weatherTimeOfDayIntensity.registerChangeCallback(m_lifetime,
                                                                        onTimeOfDaySettingChanged);

    m_observer.sig2_tick.connect(m_lifetime, [this](const MumeMoment &moment) {
        m_clockMoment = moment;
        m_clockAnchorTime = std::chrono::steady_clock::now();
        const auto dawnDusk = MumeClock::getDawnDusk(moment.month);
        if (!m_previewMoment && !m_data.isEmpty()
            && (m_skyChanging || moment.hour == dawnDusk.dawnHour - 1
                || moment.hour == dawnDusk.duskHour - 1))
            m_animationManager.requestUpdate();
    });

    m_animationManager.registerCallback(m_lifetime, [this]() {
        return isAnimating() ? FrameManager::AnimationStatusEnum::Continue
                             : FrameManager::AnimationStatusEnum::Stop;
    });

    m_gl.getUboManager().registerRebuildFunction(Legacy::SharedVboEnum::WeatherBlock,
                                                 [this](Legacy::Functions &funcs) {
                                                     m_gl.getUboManager()
                                                         .sync<Legacy::SharedVboEnum::WeatherBlock>(
                                                             funcs);
                                                 });
}

GLWeather::~GLWeather()
{
    m_gl.getUboManager().unregisterRebuildFunction(Legacy::SharedVboEnum::WeatherBlock);
}

void GLWeather::cleanup()
{
    m_particles.reset();
    m_atmosphere.reset();
    m_timeOfDay.reset();
}

void GLWeather::updateFromGame()
{
    switch (m_observer.getWeather()) {
    case PromptWeatherEnum::NICE:
        m_gameCloudsIntensity = 0.0f;
        m_gameRainIntensity = 0.0f;
        m_gameSnowIntensity = 0.0f;
        break;
    case PromptWeatherEnum::CLOUDS:
        m_gameCloudsIntensity = 0.5f;
        m_gameRainIntensity = 0.0f;
        m_gameSnowIntensity = 0.0f;
        break;
    case PromptWeatherEnum::RAIN:
        m_gameCloudsIntensity = 0.8f;
        m_gameRainIntensity = 0.5f;
        m_gameSnowIntensity = 0.0f;
        break;
    case PromptWeatherEnum::HEAVY_RAIN:
        m_gameCloudsIntensity = 1.0f;
        m_gameRainIntensity = 1.0f;
        m_gameSnowIntensity = 0.0f;
        break;
    case PromptWeatherEnum::SNOW:
        m_gameCloudsIntensity = 0.8f;
        m_gameRainIntensity = 0.0f;
        m_gameSnowIntensity = 0.8f;
        break;
    }

    switch (m_observer.getFog()) {
    case PromptFogEnum::NO_FOG:
        m_gameFogIntensity = 0.0f;
        break;
    case PromptFogEnum::LIGHT_FOG:
        m_gameFogIntensity = 0.5f;
        break;
    case PromptFogEnum::HEAVY_FOG:
        m_gameFogIntensity = 1.0f;
        break;
    }
}

void GLWeather::updateTargets()
{
    const auto &canvasSettings = getConfig().canvas;
    auto &weather = m_gl.getUboManager().get<Legacy::SharedVboEnum::WeatherBlock>();

    weather.targets.x = m_gameRainIntensity
                        * (static_cast<float>(canvasSettings.weatherPrecipitationIntensity.get())
                           / 50.0f);
    weather.targets.y = m_gameSnowIntensity
                        * (static_cast<float>(canvasSettings.weatherPrecipitationIntensity.get())
                           / 50.0f);
    weather.targets.z = m_gameCloudsIntensity
                        * (static_cast<float>(canvasSettings.weatherAtmosphereIntensity.get())
                           / 50.0f);
    weather.targets.w = m_gameFogIntensity
                        * (static_cast<float>(canvasSettings.weatherAtmosphereIntensity.get())
                           / 50.0f);
    weather.timeOfDay.w = m_gameTimeOfDayIntensity
                          * (static_cast<float>(canvasSettings.weatherTimeOfDayIntensity.get())
                             / 50.0f);
}

void GLWeather::setPreviewMoment(std::optional<MumeMoment> moment)
{
    m_previewMoment = std::move(moment);
    m_animationManager.requestUpdate();
}

void GLWeather::updateSkyTint()
{
    m_skyTint = glm::vec4{0.f, 0.f, 0.f, -1.f};
    m_skyChanging = false;
    const bool preview = m_previewMoment.has_value();
    const auto &moment = preview ? m_previewMoment : m_clockMoment;
    if (!moment || (!preview && m_observer.getTimeOfDay() == MumeTimeEnum::UNKNOWN))
        return;
    const auto dawnDusk = MumeClock::getDawnDusk(moment->month);
    // One real second is one MUME minute. Extrapolate only within the latest
    // tick; paused preview time stays exactly at its selected minute.
    const float fraction = preview
                               ? 0.f
                               : std::clamp(static_cast<float>(std::chrono::duration<double>(
                                                                   std::chrono::steady_clock::now()
                                                                   - m_clockAnchorTime)
                                                                   .count()),
                                            0.f,
                                            1.f);
    const double hour = static_cast<double>(moment->hour)
                        + (static_cast<double>(moment->minute) + static_cast<double>(fraction))
                              / 60.0;
    const bool brightMoon = preview ? moment->moonVisibility() == MumeMoonVisibilityEnum::BRIGHT
                                          && !moment->isMoonBelowHorizon()
                                    : m_moonVisibility == MumeMoonVisibilityEnum::BRIGHT;
    auto night = XNamedColor{brightMoon ? NamedColorEnum::WEATHER_NIGHT_MOON
                                        : NamedColorEnum::WEATHER_NIGHT}
                     .getColor()
                     .getVec4();
    if (!preview) {
        // Preserve the existing two-second moon-brightness transition when
        // both ends of the old weather transition are night colours.
        const auto &weather = m_gl.getUboManager().get<Legacy::SharedVboEnum::WeatherBlock>();
        const auto start = static_cast<NamedColorEnum>(static_cast<int>(weather.timeOfDay.x));
        const auto target = static_cast<NamedColorEnum>(static_cast<int>(weather.timeOfDay.y));
        const auto isNight = [](NamedColorEnum color) {
            return color == NamedColorEnum::WEATHER_NIGHT
                   || color == NamedColorEnum::WEATHER_NIGHT_MOON;
        };
        if (isNight(start) && isNight(target)) {
            const auto a = XNamedColor{start}.getColor().getVec4();
            const auto b = XNamedColor{target}.getColor().getVec4();
            const float progress = std::clamp((m_animationManager.getElapsedTime()
                                               - weather.config.y)
                                                  / WeatherConstants::TRANSITION_DURATION,
                                              0.f,
                                              1.f);
            const auto mixed = terrain_twilight::blend({a.r, a.g, a.b, a.a},
                                                       {b.r, b.g, b.b, b.a},
                                                       progress);
            night = glm::vec4{mixed[0], mixed[1], mixed[2], mixed[3]};
        }
    }
    const auto sky = terrain_twilight::sample(hour,
                                              dawnDusk.dawnHour,
                                              dawnDusk.duskHour,
                                              {night.r, night.g, night.b, night.a});
    const float intensity = static_cast<float>(getConfig().canvas.weatherTimeOfDayIntensity.get())
                            / 50.f;
    m_skyTint = glm::vec4{sky.tint[0], sky.tint[1], sky.tint[2], sky.tint[3] * intensity};
    m_skyChanging = sky.changing && !preview && intensity > 0.f;
}

void GLWeather::update()
{
    updateSkyTint();
    const auto &weather = m_gl.getUboManager().get<Legacy::SharedVboEnum::WeatherBlock>();

    m_currentRainIntensity = applyTransition(weather.config.x,
                                             weather.intensities.x,
                                             weather.targets.x);
    m_currentSnowIntensity = applyTransition(weather.config.x,
                                             weather.intensities.y,
                                             weather.targets.y);
    m_currentCloudsIntensity = applyTransition(weather.config.x,
                                               weather.intensities.z,
                                               weather.targets.z);
    m_currentFogIntensity = applyTransition(weather.config.x,
                                            weather.intensities.w,
                                            weather.targets.w);

    m_currentTimeOfDayIntensity = applyTransition(weather.config.y,
                                                  weather.timeOfDay.z,
                                                  weather.timeOfDay.w);
}

bool GLWeather::isAnimating() const
{
    const bool activePrecipitation = m_currentRainIntensity > 0.0f || m_currentSnowIntensity > 0.0f;
    const bool activeAtmosphere = m_currentCloudsIntensity > 0.0f || m_currentFogIntensity > 0.0f;
    return m_skyChanging || isTransitioning() || activePrecipitation || activeAtmosphere;
}

bool GLWeather::isTransitioning() const
{
    const float animTime = m_animationManager.getElapsedTime();
    const auto &wb = m_gl.getUboManager().get<Legacy::SharedVboEnum::WeatherBlock>();
    const float duration = wb.config.z;

    const bool weatherTransitioning = (animTime - wb.config.x < duration);
    const bool timeOfDayTransitioning = (animTime - wb.config.y < duration);
    return weatherTransitioning || timeOfDayTransitioning;
}

void GLWeather::syncWeatherAtmosphere()
{
    auto &funcs = deref(m_gl.getSharedFunctions(Badge<GLWeather>{}));
    auto &ubo = m_gl.getUboManager();
    ubo.syncFields<Legacy::SharedVboEnum::WeatherBlock>(funcs,
                                                        &Legacy::WeatherBlock::config,
                                                        &Legacy::WeatherBlock::intensities,
                                                        &Legacy::WeatherBlock::targets);
}

void GLWeather::syncWeatherTimeOfDay()
{
    auto &funcs = deref(m_gl.getSharedFunctions(Badge<GLWeather>{}));
    auto &ubo = m_gl.getUboManager();
    ubo.syncFields<Legacy::SharedVboEnum::WeatherBlock>(funcs,
                                                        &Legacy::WeatherBlock::config,
                                                        &Legacy::WeatherBlock::timeOfDay);
}

NamedColorEnum GLWeather::getCurrentColorIdx() const
{
    switch (m_currentTimeOfDay) {
    case MumeTimeEnum::DAY:
        return NamedColorEnum::TRANSPARENT;
    case MumeTimeEnum::NIGHT:
        return (m_moonVisibility == MumeMoonVisibilityEnum::BRIGHT)
                   ? NamedColorEnum::WEATHER_NIGHT_MOON
                   : NamedColorEnum::WEATHER_NIGHT;
    case MumeTimeEnum::DAWN:
        return NamedColorEnum::WEATHER_DAWN;
    case MumeTimeEnum::DUSK:
        return NamedColorEnum::WEATHER_DUSK;
    case MumeTimeEnum::UNKNOWN:
        return NamedColorEnum::TRANSPARENT;
    }
    return NamedColorEnum::TRANSPARENT;
}

float GLWeather::applyTransition(const float startTime,
                                 const float startVal,
                                 const float targetVal) const
{
    const float t = (m_animationManager.getElapsedTime() - startTime)
                    / WeatherConstants::TRANSITION_DURATION;
    const float factor = std::clamp(t, 0.0f, 1.0f);
    return glm::lerp(startVal, targetVal, factor);
}

template<typename T>
T GLWeather::applyTransition(float startTime, T startVal, T targetVal) const
{
    if (startVal == targetVal) {
        return startVal;
    }
    const float t = (m_animationManager.getElapsedTime() - startTime)
                    / WeatherConstants::TRANSITION_DURATION;
    return (t >= 1.0f) ? targetVal : startVal;
}

template<typename... Pairs>
void GLWeather::startTransitions(float &startTime, Pairs... pairs)
{
    float oldStartTime = startTime;
    startTime = m_animationManager.getElapsedTime();
    (..., (pairs.start = applyTransition(oldStartTime, pairs.start, pairs.target)));
}

template void GLWeather::startTransitions(float &startTime,
                                          TransitionPair<float> p1,
                                          TransitionPair<float> p2,
                                          TransitionPair<float> p3,
                                          TransitionPair<float> p4);

template void GLWeather::startTransitions(float &startTime,
                                          TransitionPair<float> p1,
                                          TransitionPair<NamedColorEnum> p2);

void GLWeather::initMeshes()
{
    if (!m_particles) {
        auto funcs = m_gl.getSharedFunctions(Badge<GLWeather>{});
        auto &shaderPrograms = funcs->getShaderPrograms();

        m_particles = std::make_unique<Legacy::WeatherParticleMesh>(
            funcs,
            shaderPrograms.getParticleSimulationShader(),
            shaderPrograms.getParticleRenderShader());
        m_atmosphere = UniqueMesh(
            std::make_unique<Legacy::AtmosphereMesh>(funcs, shaderPrograms.getAtmosphereShader()));
        m_timeOfDay = UniqueMesh(
            std::make_unique<Legacy::TimeOfDayMesh>(funcs, shaderPrograms.getTimeOfDayShader()));
    }
}

void GLWeather::prepare()
{
    initMeshes();

    auto &funcs = deref(m_gl.getSharedFunctions(Badge<GLWeather>{}));
    m_gl.getUboManager().bind(funcs, Legacy::SharedVboEnum::CameraBlock);
    m_gl.getUboManager().bind(funcs, Legacy::SharedVboEnum::WeatherBlock);
}

void GLWeather::render(const GLRenderState &rs)
{
    // 1. Render Particles (Simulation + Rendering)
    const float rainMax = m_currentRainIntensity;
    const float snowMax = m_currentSnowIntensity;
    if (rainMax > 0.0f || snowMax > 0.0f) {
        auto particleRs = rs.withBlend(BlendModeEnum::MAX_ALPHA);

        if (m_particles) {
            m_particles->setIntensity(std::max(rainMax, snowMax));
            m_particles->render(particleRs);
        }
    }

    // 2. Render Atmosphere (TimeOfDay + Atmosphere)
    const auto atmosphereRs = rs.withBlend(BlendModeEnum::TRANSPARENCY)
                                  .withDepthFunction(std::nullopt);

    // TimeOfDay
    const auto &wb = m_gl.getUboManager().get<Legacy::SharedVboEnum::WeatherBlock>();
    const auto currentStartColor = static_cast<NamedColorEnum>(static_cast<int>(wb.timeOfDay.x));
    const bool useSkyTint = m_skyTint.a >= 0.f;
    if (rs.uniforms.roomLightParams.x > 0.f
        || (useSkyTint ? m_skyTint.a > 0.f
                       : m_currentTimeOfDay != MumeTimeEnum::DAY
                             || currentStartColor != NamedColorEnum::TRANSPARENT
                             || m_currentTimeOfDayIntensity > 0.f)) {
        if (m_timeOfDay)
            m_timeOfDay.render(atmosphereRs.withSkyTint(m_skyTint));
    }

    // Atmosphere
    const float cloudMax = m_currentCloudsIntensity;
    const float fogMax = m_currentFogIntensity;
    if ((cloudMax > 0.0f || fogMax > 0.0f) && m_atmosphere) {
        m_atmosphere.render(atmosphereRs.withTexture0(m_textures.noise->getId()));
    }
}
