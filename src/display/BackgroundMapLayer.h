#pragma once
// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 The MMapper Authors

#include "../global/AsyncTasks.h"
#include "../global/ChangeMonitor.h"
#include "BackgroundMapData.h"

#include <functional>
#include <map>
#include <optional>

#include <QObject>

class OpenGL;

// A display-only layer: never registered with MapFrontend, game observation,
// selection, undo, pathfinding, export or save. CPU caches outlive GL contexts.
class NODISCARD_QOBJECT BackgroundMapLayer final : public QObject
{
    Q_OBJECT

private:
    ChangeMonitor::Lifetime m_lifetime;
    std::function<void()> m_requestUpdate;
    std::optional<async_tasks::AsyncTaskHandle> m_mapTask;
    std::optional<async_tasks::AsyncTaskHandle> m_textureTask;
    std::optional<async_tasks::AsyncTaskHandle> m_shadowTask;
    size_t m_mapGeneration = 0;
    size_t m_textureGeneration = 0;
    size_t m_shadowGeneration = 0;
    backgroundmap::Geometry m_geometry;
    std::vector<QImage> m_images;
    backgroundmap::ShadowGeometry m_shadow;
    std::optional<Map> m_shadowMap;
    bool m_geometryDirty = true;
    bool m_texturesDirty = true;
    bool m_shadowDirty = true;
    bool m_started = false;
    std::map<backgroundmap::ChunkKey, UniqueMesh> m_meshes;
    std::map<int, UniqueMesh> m_shadowMeshes;
    SharedMMTexture m_texture;
    MMTextureId m_textureId = INVALID_MM_TEXTURE_ID;

public:
    explicit BackgroundMapLayer(std::function<void()> requestUpdate);
    ~BackgroundMapLayer() final;

    void start();
    // Both rendering and cleanup require the canvas's current GL context.
    void render(OpenGL &gl, int layer, const glm::mat4 &viewProjection, const Map &foreground);
    void cleanupGL(OpenGL &gl);

private:
    void reloadMap();
    void reloadTextures();
    void updateShadow(const Map &foreground);
    void uploadTextures(OpenGL &gl);

signals:
    void sig_log(const QString &message);
};
