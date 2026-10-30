// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 The MMapper Authors

#include "BackgroundMapLayer.h"

#include "../configuration/configuration.h"
#include "../opengl/OpenGL.h"
#include "Textures.h"

#include <algorithm>
#include <array>
#include <exception>
#include <utility>

#include <QPointer>

namespace {
void cancel(std::optional<async_tasks::AsyncTaskHandle> &task)
{
    if (task) {
        task->requestCancel();
        task.reset();
    }
}

template<typename T>
struct WorkResult final
{
    T data;
    QString error;
};
} // namespace

BackgroundMapLayer::BackgroundMapLayer(std::function<void()> requestUpdate)
    : m_requestUpdate{std::move(requestUpdate)}
{
    auto &settings = setConfig().canvas;
    const auto mapChanged = [this]() {
        if (m_started) {
            reloadMap();
        }
        m_requestUpdate();
    };
    settings.backgroundMapFile.registerChangeCallback(m_lifetime, mapChanged);
    settings.backgroundMapReload.registerChangeCallback(m_lifetime, mapChanged);
    const auto texturesChanged = [this]() {
        if (m_started) {
            reloadTextures();
        }
        m_requestUpdate();
    };
    settings.backgroundMapTileset.registerChangeCallback(m_lifetime, texturesChanged);
    settings.backgroundMapReload.registerChangeCallback(m_lifetime, texturesChanged);
    settings.backgroundMapTilesDirectory.registerChangeCallback(m_lifetime, texturesChanged);
    settings.backgroundMapSoften.registerChangeCallback(m_lifetime, texturesChanged);
    const auto repaint = [this]() { m_requestUpdate(); };
    settings.backgroundMapEnabled.registerChangeCallback(m_lifetime, [this]() {
        if (m_started && getConfig().canvas.backgroundMapEnabled.get()) {
            if (m_geometry.empty() && !m_mapTask) {
                reloadMap();
            }
            if (m_images.empty() && !m_textureTask) {
                reloadTextures();
            }
        }
        m_requestUpdate();
    });
    settings.backgroundMapOpacity.registerChangeCallback(m_lifetime, repaint);
    settings.backgroundMapShadow.registerChangeCallback(m_lifetime, repaint);
}

BackgroundMapLayer::~BackgroundMapLayer()
{
    cancel(m_mapTask);
    cancel(m_textureTask);
    cancel(m_shadowTask);
}

void BackgroundMapLayer::start()
{
    if (std::exchange(m_started, true)) {
        return;
    }
    if (getConfig().canvas.backgroundMapEnabled.get()) {
        reloadMap();
        reloadTextures();
    }
}

void BackgroundMapLayer::reloadMap()
{
    cancel(m_mapTask);
    const auto generation = ++m_mapGeneration;
    m_geometry.clear();
    m_geometryDirty = true;
    const auto &settings = getConfig().canvas;
    const auto filename = settings.backgroundMapFile.get();
    if (!settings.backgroundMapEnabled.get() || filename.isEmpty()) {
        return;
    }
    auto result = std::make_shared<WorkResult<backgroundmap::Geometry>>();
    const QPointer<BackgroundMapLayer> self(this);
    m_mapTask = async_tasks::startAsyncTask2(
        AsyncTaskTypeEnum::IO,
        AllowCancelEnum::Allow,
        "Load background terrain",
        [filename, result](const std::shared_ptr<ProgressCounter> &pc) {
            try {
                result->data = backgroundmap::loadGeometry(filename, pc);
            } catch (const ProgressCanceledException &) {
                throw;
            } catch (const std::exception &error) {
                result->error = QString::fromUtf8(error.what());
            }
        },
        [self, generation, result](const std::shared_ptr<ProgressCounter> & /*pc*/) {
            if (!self || generation != self->m_mapGeneration) {
                return;
            }
            self->m_mapTask.reset();
            self->m_geometry = std::move(result->data);
            self->m_geometryDirty = true;
            emit self->sig_log(result->error.isEmpty()
                                   ? QStringLiteral("Background terrain loaded (read-only)")
                                   : QStringLiteral("Background map: %1").arg(result->error));
            self->m_requestUpdate();
        });
}

void BackgroundMapLayer::reloadTextures()
{
    cancel(m_textureTask);
    const auto generation = ++m_textureGeneration;
    const auto &settings = getConfig().canvas;
    if (!settings.backgroundMapEnabled.get()) {
        // An old set must not reappear when enabled with new preferences.
        m_images.clear();
        m_texturesDirty = true;
        return;
    }
    const auto files = backgroundmap::textureFiles(settings.backgroundMapTileset.get(),
                                                   settings.backgroundMapTilesDirectory.get());
    const bool soften = settings.backgroundMapSoften.get();
    auto result = std::make_shared<WorkResult<std::vector<QImage>>>();
    const QPointer<BackgroundMapLayer> self(this);
    m_textureTask = async_tasks::startAsyncTask(
        AsyncTaskTypeEnum::Task,
        AllowCancelEnum::Allow,
        "Prepare background tiles",
        [files, soften, result](ProgressCounter &pc) {
            try {
                result->data = backgroundmap::loadTextures(files, soften, pc);
            } catch (const ProgressCanceledException &) {
                throw;
            } catch (const std::exception &error) {
                result->error = QString::fromUtf8(error.what());
            }
        },
        [self, generation, result]() {
            if (!self || generation != self->m_textureGeneration) {
                return;
            }
            self->m_textureTask.reset();
            self->m_images = std::move(result->data);
            self->m_texturesDirty = true;
            if (!result->error.isEmpty()) {
                emit self->sig_log(QStringLiteral("Background tiles: %1").arg(result->error));
            }
            self->m_requestUpdate();
        });
}

void BackgroundMapLayer::updateShadow(const Map &foreground, const RoomVisibility &visibility)
{
    if (m_shadowMap && m_shadowMap->isSamePointer(foreground)
        && m_shadowVisibility.matches(visibility)) {
        return;
    }
    cancel(m_shadowTask);
    const auto generation = ++m_shadowGeneration;
    m_shadowMap = foreground;
    m_shadowVisibility = visibility;
    m_shadow.clear();
    m_shadowDirty = true;
    auto result = std::make_shared<backgroundmap::ShadowGeometry>();
    const QPointer<BackgroundMapLayer> self(this);
    m_shadowTask = async_tasks::startAsyncTask(
        AsyncTaskTypeEnum::Task,
        AllowCancelEnum::Allow,
        "Prepare foreground boundary shadow",
        [foreground, visibility, result](ProgressCounter &pc) {
            *result = backgroundmap::buildShadow(foreground, pc, visibility);
        },
        [self, generation, result]() {
            if (!self || generation != self->m_shadowGeneration) {
                return;
            }
            self->m_shadowTask.reset();
            self->m_shadow = std::move(*result);
            self->m_shadowDirty = true;
            self->m_requestUpdate();
        });
}

void BackgroundMapLayer::uploadTextures(OpenGL &gl)
{
    if (!std::exchange(m_texturesDirty, false)) {
        return;
    }
    if (m_textureId == INVALID_MM_TEXTURE_ID) {
        m_textureId = allocateTextureId();
    }
    gl.setTextureLookup(m_textureId, {});
    m_texture.reset();
    if (m_images.empty()) {
        return;
    }
    auto texture = MMTexture::alloc(
        QOpenGLTexture::Target2DArray,
        [this](QOpenGLTexture &tex) {
            tex.setFormat(QOpenGLTexture::RGBA8_UNorm);
            tex.setSize(backgroundmap::TILE_SIZE, backgroundmap::TILE_SIZE, 1);
            tex.setLayers(static_cast<int>(m_images.size()));
            tex.setMipLevels(tex.maximumMipLevels());
            tex.setAutoMipMapGenerationEnabled(false);
            tex.setWrapMode(QOpenGLTexture::ClampToEdge);
            tex.setMinMagFilters(QOpenGLTexture::LinearMipMapLinear, QOpenGLTexture::Linear);
            tex.allocateStorage(QOpenGLTexture::RGBA, QOpenGLTexture::UInt8);
        },
        true);
    texture->setId(m_textureId);
    gl.setTextureLookup(m_textureId, texture);
    for (size_t i = 0; i < m_images.size(); ++i) {
        gl.uploadArrayLayer(texture, static_cast<int>(i), std::array{m_images[i]});
    }
    gl.generateMipmaps(texture);
    m_texture = std::move(texture);
}

void BackgroundMapLayer::render(OpenGL &gl,
                                const int layer,
                                const glm::mat4 &viewProjection,
                                const Map &foreground,
                                const RoomVisibility &visibility)
{
    const auto &settings = getConfig().canvas;
    if (!settings.backgroundMapEnabled.get()) {
        return;
    }
    if (std::exchange(m_geometryDirty, false)) {
        m_meshes.clear();
    }
    uploadTextures(gl);
    if (!m_texture || m_geometry.empty()) {
        return;
    }
    const float opacity = static_cast<float>(std::clamp(settings.backgroundMapOpacity.get(), 0, 100))
                          / 100.f;
    if (opacity <= 0.f) {
        return;
    }
    const auto state = GLRenderState()
                           .withBlend(BlendModeEnum::TRANSPARENCY)
                           .withColor(Color{1.f, 1.f, 1.f, opacity});
    for (const auto &[key, vertices] : m_geometry) {
        if (std::get<0>(key) != layer || !backgroundmap::isChunkVisible(key, viewProjection)) {
            continue;
        }
        auto &mesh = m_meshes[key];
        if (!mesh) {
            mesh = gl.createRoomQuadTexBatch(vertices, m_textureId);
        }
        mesh.render(state);
    }
    if (settings.backgroundMapShadow.get()) {
        // Foreground changes invalidate only this small boundary cache. They
        // never touch the background geometry, images, textures or meshes.
        updateShadow(foreground, visibility);
        if (std::exchange(m_shadowDirty, false)) {
            m_shadowMeshes.clear();
        }
        const auto it = m_shadow.find(layer);
        if (it != m_shadow.end() && !it->second.empty()) {
            auto &mesh = m_shadowMeshes[layer];
            if (!mesh) {
                mesh = gl.createColoredQuadBatch(it->second);
            }
            mesh.render(GLRenderState().withBlend(BlendModeEnum::TRANSPARENCY));
        }
    }
}

void BackgroundMapLayer::cleanupGL(OpenGL &gl)
{
    m_meshes.clear();
    m_shadowMeshes.clear();
    if (m_textureId != INVALID_MM_TEXTURE_ID) {
        gl.setTextureLookup(m_textureId, {});
    }
    m_texture.reset();
    m_textureId = INVALID_MM_TEXTURE_ID;
    m_geometryDirty = m_texturesDirty = m_shadowDirty = true;
}
