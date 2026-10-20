#pragma once
// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2021 The MMapper Authors

#include "Connections.h"
#include "MapCanvasData.h"

#include <map>

template<typename T>
using RoomTintArray = EnumIndexedArray<T, RoomTintEnum, NUM_ROOM_TINTS>;

struct NODISCARD LayerMeshes final
{
    UniqueMeshVector terrain;
    UniqueMeshVector forestTerrain;
    UniqueMesh organicGroundBase;
    UniqueMesh groundSprites;
    UniqueMesh terrainSprites;
    UniqueMesh terrainEdges;
    UniqueMesh terrainWalls;
    UniqueMesh roofCaps;
    UniqueMesh doorSprites;
    UniqueMesh warningSprites;
    UniqueMesh wallShadows;
    UniqueMesh wallGrime;
    UniqueMeshVector legacyTrails;
    UniqueMeshVector trails;
    RoomTintArray<UniqueMesh> tints;
    UniqueMeshVector overlays;
    UniqueMeshVector doors;
    UniqueMeshVector legacyDoorLines;
    UniqueMeshVector walls;
    UniqueMeshVector dottedWalls;
    UniqueMeshVector upDownExits;
    UniqueMeshVector streamIns;
    UniqueMeshVector streamOuts;
    UniqueMesh layerBoost;
    bool isValid = false;

    LayerMeshes() = default;
    DEFAULT_MOVES_DELETE_COPIES(LayerMeshes);
    ~LayerMeshes() = default;

    void render(int thisLayer,
                int focusedLayer,
                bool drawTerrainSprites,
                const glm::vec4 &shadow,
                float day,
                const glm::ivec4 &doorState);
    explicit operator bool() const { return isValid; }
};

struct NODISCARD LayerMeshesIntermediate final
{
    using Fn = std::function<UniqueMesh(OpenGL &)>;
    using FnVec = std::vector<Fn>;
    FnVec terrain;
    FnVec forestTerrain;
    Fn organicGroundBase;
    Fn groundSprites;
    Fn terrainSprites;
    Fn terrainEdges;
    Fn terrainWalls;
    Fn roofCaps;
    Fn doorSprites;
    Fn warningSprites;
    Fn wallShadows;
    Fn wallGrime;
    FnVec legacyTrails;
    FnVec trails;
    RoomTintArray<Fn> tints;
    FnVec overlays;
    FnVec doors;
    FnVec legacyDoorLines;
    FnVec walls;
    FnVec dottedWalls;
    FnVec upDownExits;
    FnVec streamIns;
    FnVec streamOuts;
    Fn layerBoost;
    bool isValid = false;

    LayerMeshesIntermediate() = default;
    DEFAULT_MOVES_DELETE_COPIES(LayerMeshesIntermediate);
    ~LayerMeshesIntermediate() = default;

    NODISCARD LayerMeshes getLayerMeshes(OpenGL &gl) const;
    explicit operator bool() const { return isValid; }
};

// This must be ordered so we can iterate over the layers from lowest to highest.
using BatchedMeshes = std::map<int, LayerMeshes>;

struct NODISCARD MapBatches final
{
    BatchedMeshes batchedMeshes;
    BatchedConnectionMeshes connectionMeshes;
    BatchedRoomNames roomNameBatches;

    MapBatches() = default;
    ~MapBatches() = default;
    DEFAULT_MOVES_DELETE_COPIES(MapBatches);
};
