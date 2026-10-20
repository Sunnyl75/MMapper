// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2019 The MMapper Authors
// Author: Nils Schimmelmann <nschimme@gmail.com> (Jahara)

#include "MapCanvasRoomDrawer.h"

#include "../configuration/NamedConfig.h"
#include "../configuration/configuration.h"
#include "../global/Array.h"
#include "../global/ConfigConsts.h"
#include "../global/EnumIndexedArray.h"
#include "../global/Flags.h"
#include "../global/RuleOf5.h"
#include "../global/progresscounter.h"
#include "../global/utils.h"
#include "../map/DoorFlags.h"
#include "../map/ExitFieldVariant.h"
#include "../map/ExitFlags.h"
#include "../map/Map.h"
#include "../map/coordinate.h"
#include "../map/enums.h"
#include "../map/exit.h"
#include "../map/infomark.h"
#include "../map/mmapper2room.h"
#include "../map/room.h"
#include "../mapdata/mapdata.h"
#include "../opengl/FontFormatFlags.h"
#include "../opengl/OpenGL.h"
#include "../opengl/OpenGLTypes.h"
#include "../opengl/legacy/VBO.h"
#include "ConnectionLineBuilder.h"
#include "MapCanvasData.h"
#include "RoadIndex.h"
#include "TerrainDoors.h"
#include "TerrainEdges.h"
#include "TerrainMaterials.h"
#include "TerrainSprites.h"
#include "TerrainWalls.h"
#include "mapcanvas.h" // hack, since we're now definining some of its symbols

#include <cassert>
#include <cstdlib>
#include <memory>
#include <optional>
#include <set>
#include <stdexcept>
#include <tuple>
#include <unordered_map>
#include <vector>

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <QColor>
#include <QMessageLogContext>
#include <QtGui/qopengl.h>
#include <QtGui>

struct NODISCARD VisitRoomOptions final
{
    SharedCanvasNamedColorOptions canvasColors;
    SharedNamedColorOptions colorSettings;
    bool drawNotMappedExits = false;
    bool organicTerrain = false;
};

enum class NODISCARD StreamTypeEnum { OutFlow, InFlow };
enum class NODISCARD WallTypeEnum { SOLID, DOTTED, DOOR };
static constexpr const size_t NUM_WALL_TYPES = 3;
DEFINE_ENUM_COUNT(WallTypeEnum, NUM_WALL_TYPES)

#define LOOKUP_COLOR(_X) (NamedColorEnum::_X)

// must be called from the main thread
NODISCARD static VisitRoomOptions getVisitRoomOptions()
{
    const auto &config = getConfig();
    auto &canvas = config.canvas;
    VisitRoomOptions result;
    result.canvasColors = canvas.clone();
    result.colorSettings = config.colorSettings.clone();
    result.drawNotMappedExits = canvas.showUnmappedExits.get();
    result.organicTerrain = canvas.organicTerrain.get();
    return result;
}

NODISCARD static NamedColorEnum getTintColor(const RoomTintEnum tint)
{
    static_assert(NUM_ROOM_TINTS == 2);
    switch (tint) {
    case RoomTintEnum::DARK:
        return NamedColorEnum::ROOM_DARK;
    case RoomTintEnum::NO_SUNDEATH:
        return NamedColorEnum::ROOM_NO_SUNDEATH;
    }
    std::abort();
}

NODISCARD static bool isTransparent(const NamedColorEnum namedColor)
{
    return namedColor == NamedColorEnum::TRANSPARENT;
}

enum class NODISCARD WallOrientationEnum { HORIZONTAL, VERTICAL };
NODISCARD static NamedColorEnum getWallNamedColorCommon(const ExitFlags flags,
                                                        const WallOrientationEnum wallOrientation)
{
    const bool isVertical = wallOrientation == WallOrientationEnum::VERTICAL;

    // Vertical colors override the horizontal case
    // REVISIT: consider using the same set and just override the color
    // using the same order of flag testing as the horizontal case.
    //
    // In other words, eliminate this `if (isVertical)` block and just use
    //   return (isVertical ? LOOKUP_COLOR(VERTICAL_COLOR_CLIMB) : LOOKUP_COLOR(WALL_COLOR_CLIMB));
    // in the appropriate places in the following chained test for flags.
    if (isVertical) {
        if (flags.isClimb()) {
            // NOTE: This color is slightly darker than WALL_COLOR_CLIMB
            return LOOKUP_COLOR(VERTICAL_COLOR_CLIMB);
        }
        /*FALL-THRU*/
    }

    if (flags.isNoFlee()) {
        return LOOKUP_COLOR(WALL_COLOR_NO_FLEE);
    } else if (flags.isRandom()) {
        return LOOKUP_COLOR(WALL_COLOR_RANDOM);
    } else if (flags.isFall() || flags.isDamage()) {
        return LOOKUP_COLOR(WALL_COLOR_FALL_DAMAGE);
    } else if (flags.isSpecial()) {
        return LOOKUP_COLOR(WALL_COLOR_SPECIAL);
    } else if (flags.isClimb()) {
        return LOOKUP_COLOR(WALL_COLOR_CLIMB);
    } else if (flags.isGuarded()) {
        return LOOKUP_COLOR(WALL_COLOR_GUARDED);
    } else if (flags.isNoMatch()) {
        return LOOKUP_COLOR(WALL_COLOR_NO_MATCH);
    } else {
        return LOOKUP_COLOR(TRANSPARENT);
    }
}

NODISCARD static NamedColorEnum getWallNamedColor(const ExitFlags flags)
{
    return getWallNamedColorCommon(flags, WallOrientationEnum::HORIZONTAL);
}

NODISCARD static NamedColorEnum getVerticalNamedColor(const ExitFlags flags)
{
    return getWallNamedColorCommon(flags, WallOrientationEnum::VERTICAL);
}

struct LayerBatchData;

struct NODISCARD TerrainAndTrail final
{
    MMTexArrayPosition terrain;
    MMTexArrayPosition trail;
};

NODISCARD static TerrainAndTrail getRoomTerrainAndTrail(const mctp::MapCanvasTexturesProxy &textures,
                                                        const RawRoom &room)
{
    const auto roomTerrainType = room.getTerrainType();
    const RoadIndexMaskEnum roadIndex = getRoadIndex(room);

    TerrainAndTrail result;
    result.terrain = (roomTerrainType == RoomTerrainEnum::ROAD) ? textures.road[roadIndex]
                                                                : textures.terrain[roomTerrainType];

    if (roadIndex != RoadIndexMaskEnum::NONE && roomTerrainType != RoomTerrainEnum::ROAD) {
        result.trail = textures.trail[roadIndex];
    }
    return result;
}

struct NODISCARD IRoomVisitorCallbacks
{
public:
    virtual ~IRoomVisitorCallbacks();

private:
    NODISCARD virtual bool virt_acceptRoom(const RoomHandle &) const = 0;

private:
    // Rooms
    virtual void virt_visitTerrainTexture(const RoomHandle &, const MMTexArrayPosition &) = 0;
    virtual void virt_visitTrailTexture(const RoomHandle &, const MMTexArrayPosition &) = 0;
    virtual void virt_visitOverlayTexture(const RoomHandle &, const MMTexArrayPosition &) = 0;
    virtual void virt_visitNamedColorTint(const RoomHandle &, RoomTintEnum) = 0;

    // Walls
    virtual void virt_visitWall(
        const RoomHandle &, ExitDirEnum, NamedColorEnum, WallTypeEnum, bool isClimb)
        = 0;

    // Streams
    virtual void virt_visitStream(const RoomHandle &, ExitDirEnum, StreamTypeEnum) = 0;

public:
    NODISCARD bool acceptRoom(const RoomHandle &room) const { return virt_acceptRoom(room); }

    // Rooms
    void visitTerrainTexture(const RoomHandle &room, const MMTexArrayPosition &tex)
    {
        virt_visitTerrainTexture(room, tex);
    }
    void visitTrailTexture(const RoomHandle &room, const MMTexArrayPosition &tex)
    {
        virt_visitTrailTexture(room, tex);
    }
    void visitOverlayTexture(const RoomHandle &room, const MMTexArrayPosition &tex)
    {
        virt_visitOverlayTexture(room, tex);
    }
    void visitNamedColorTint(const RoomHandle &room, const RoomTintEnum tint)
    {
        virt_visitNamedColorTint(room, tint);
    }

    // Walls
    void visitWall(const RoomHandle &room,
                   const ExitDirEnum dir,
                   const NamedColorEnum color,
                   const WallTypeEnum wallType,
                   const bool isClimb)
    {
        virt_visitWall(room, dir, color, wallType, isClimb);
    }

    // Streams
    void visitStream(const RoomHandle &room, const ExitDirEnum dir, StreamTypeEnum streamType)
    {
        virt_visitStream(room, dir, streamType);
    }
};

IRoomVisitorCallbacks::~IRoomVisitorCallbacks() = default;

static void visitRoom(const RoomHandle &room,
                      const mctp::MapCanvasTexturesProxy &textures,
                      IRoomVisitorCallbacks &callbacks,
                      const VisitRoomOptions &visitRoomOptions)
{
    if (!callbacks.acceptRoom(room)) {
        return;
    }

    // const auto &pos = room.getPosition();
    const bool isDark = room.getLightType() == RoomLightEnum::DARK;
    const bool hasNoSundeath = room.getSundeathType() == RoomSundeathEnum::NO_SUNDEATH;
    const bool notRideable = room.getRidableType() == RoomRidableEnum::NOT_RIDABLE;
    const auto terrainAndTrail = getRoomTerrainAndTrail(textures, room.getRaw());
    const RoomMobFlags mf = room.getMobFlags();
    const RoomLoadFlags lf = room.getLoadFlags();

    callbacks.visitTerrainTexture(room, terrainAndTrail.terrain);

    if (auto trail = terrainAndTrail.trail; trail.array != INVALID_MM_TEXTURE_ID) {
        callbacks.visitTrailTexture(room, trail);
    }

    if (isDark) {
        callbacks.visitNamedColorTint(room, RoomTintEnum::DARK);
    } else if (hasNoSundeath) {
        callbacks.visitNamedColorTint(room, RoomTintEnum::NO_SUNDEATH);
    }

    mf.for_each([&room, &textures, &callbacks](const RoomMobFlagEnum flag) -> void {
        callbacks.visitOverlayTexture(room, textures.mob[flag]);
    });

    lf.for_each([&room, &textures, &callbacks](const RoomLoadFlagEnum flag) -> void {
        callbacks.visitOverlayTexture(room, textures.load[flag]);
    });

    if (notRideable) {
        callbacks.visitOverlayTexture(room, textures.no_ride);
    }

    const Map &map = room.getMap();
    const auto drawInFlow = [&map, &room, &callbacks](const RawExit &exit,
                                                      const ExitDirEnum &dir) -> void {
        // For each incoming connections
        for (const RoomId targetRoomId : exit.getIncomingSet()) {
            const auto &targetRoom = map.getRoomHandle(targetRoomId);
            for (const ExitDirEnum targetDir : ALL_EXITS_NESWUD) {
                const auto &targetExit = targetRoom.getExit(targetDir);
                const ExitFlags flags = targetExit.getExitFlags();
                if (flags.isFlow() && targetExit.containsOut(room.getId())) {
                    callbacks.visitStream(room, dir, StreamTypeEnum::InFlow);
                    return;
                }
            }
        }
    };

    // drawExit()

    // FIXME: This requires a map update.
    // REVISIT: The logic of drawNotMappedExits seems a bit wonky.
    for (const ExitDirEnum dir : ALL_EXITS_NESW) {
        const auto &exit = room.getExit(dir);
        const ExitFlags flags = exit.getExitFlags();
        const auto isExit = flags.isExit();
        const auto isDoor = flags.isDoor();

        const bool isClimb = flags.isClimb();

        // FIXME: This requires a map update.
        // TODO: make "not mapped" exits a separate mesh;
        // except what should we do for the "else" case?
        if (visitRoomOptions.drawNotMappedExits && exit.exitIsUnmapped()) {
            callbacks.visitWall(room,
                                dir,
                                LOOKUP_COLOR(WALL_COLOR_NOT_MAPPED),
                                WallTypeEnum::DOTTED,
                                isClimb);
        } else {
            const auto namedColor = getWallNamedColor(flags);
            if (!isTransparent(namedColor)) {
                callbacks.visitWall(room, dir, namedColor, WallTypeEnum::DOTTED, isClimb);
            }

            if (flags.isFlow()) {
                callbacks.visitStream(room, dir, StreamTypeEnum::OutFlow);
            }
        }

        // wall
        if (!isExit || isDoor) {
            if (!isDoor && !exit.outIsEmpty()) {
                callbacks.visitWall(room,
                                    dir,
                                    LOOKUP_COLOR(WALL_COLOR_BUG_WALL_DOOR),
                                    WallTypeEnum::DOTTED,
                                    isClimb);
            } else {
                callbacks.visitWall(room,
                                    dir,
                                    LOOKUP_COLOR(WALL_COLOR_REGULAR_EXIT),
                                    WallTypeEnum::SOLID,
                                    isClimb);
            }
        }
        // door
        if (isDoor) {
            callbacks.visitWall(room,
                                dir,
                                LOOKUP_COLOR(WALL_COLOR_REGULAR_EXIT),
                                WallTypeEnum::DOOR,
                                isClimb);
        }

        if (!exit.inIsEmpty()) {
            drawInFlow(exit, dir);
        }
    }

    // drawVertical
    for (const ExitDirEnum dir : {ExitDirEnum::UP, ExitDirEnum::DOWN}) {
        const auto &exit = room.getExit(dir);
        const auto &flags = exit.getExitFlags();
        const bool isClimb = flags.isClimb();

        if (visitRoomOptions.drawNotMappedExits && flags.isUnmapped()) {
            callbacks.visitWall(room,
                                dir,
                                LOOKUP_COLOR(WALL_COLOR_NOT_MAPPED),
                                WallTypeEnum::DOTTED,
                                isClimb);
            continue;
        } else if (!flags.isExit()) {
            continue;
        }

        // NOTE: in the "old" version, this falls-thru and the custom color is overwritten
        // by the regular exit; so using if-else here is a bug fix.
        const auto namedColor = getVerticalNamedColor(flags);
        if (!isTransparent(namedColor)) {
            callbacks.visitWall(room, dir, namedColor, WallTypeEnum::DOTTED, isClimb);
        } else {
            callbacks.visitWall(room,
                                dir,
                                LOOKUP_COLOR(VERTICAL_COLOR_REGULAR_EXIT),
                                WallTypeEnum::SOLID,
                                isClimb);
        }

        if (flags.isDoor()) {
            callbacks.visitWall(room,
                                dir,
                                LOOKUP_COLOR(WALL_COLOR_REGULAR_EXIT),
                                WallTypeEnum::DOOR,
                                isClimb);
        }

        if (flags.isFlow()) {
            callbacks.visitStream(room, dir, StreamTypeEnum::OutFlow);
        }

        if (!exit.inIsEmpty()) {
            drawInFlow(exit, dir);
        }
    }
}

static void visitRooms(const RoomVector &rooms,
                       const mctp::MapCanvasTexturesProxy &textures,
                       IRoomVisitorCallbacks &callbacks,
                       const VisitRoomOptions &visitRoomOptions)
{
    DECL_TIMER(t, __FUNCTION__);
    for (const auto &room : rooms) {
        visitRoom(room, textures, callbacks, visitRoomOptions);
    }
}

struct NODISCARD RoomTint final
{
    Coordinate coord;
    NamedColorEnum color = NamedColorEnum::DEFAULT;

    explicit RoomTint(const Coordinate input_coord, const NamedColorEnum input_color)
        : coord{input_coord}
        , color{input_color}
    {}
};

struct NODISCARD RoomTex final
{
    Coordinate coord;
    MMTexArrayPosition pos;

    explicit RoomTex(const Coordinate input_coord, const MMTexArrayPosition &input_pos)
        : coord{input_coord}
        , pos{input_pos}
    {
        if (input_pos.array == INVALID_MM_TEXTURE_ID)
            throw std::invalid_argument("input_pos");
    }

    NODISCARD static bool isPartitioned(const RoomTex &a, const RoomTex &b)
    {
        assert(a.pos.array == b.pos.array);
        return a.pos.array < b.pos.array;
    }
};

struct NODISCARD ColoredRoomTex final
{
    Coordinate coord;
    MMTexArrayPosition pos;
    NamedColorEnum colorId = NamedColorEnum::DEFAULT;

    explicit ColoredRoomTex(const Coordinate input_coord,
                            const MMTexArrayPosition input_pos,
                            const NamedColorEnum input_color)
        : coord{input_coord}
        , pos{input_pos}
        , colorId{input_color}
    {}
};

struct NODISCARD RoomTintVector final : public std::vector<RoomTint>
{};

// Caution: Although O(n) partitioning into an array indexed by constant number of texture IDs
// is theoretically faster than O(n log n) sorting, one naive attempt to prematurely optimize
// this code resulted in a 50x slow-down.
//
// Note: sortByTexture() probably won't ever be a performance bottleneck for the default map,
// since at the time of this comment, the full O(n log n) vector sort only takes up about 2%
// of the total runtime of the mesh generation.
//
// Conclusion: Look elsewhere for optimization opportunities -- at least until profiling says
// that sorting is at significant fraction of the total runtime.
struct NODISCARD RoomTexVector final : public std::vector<RoomTex>
{
private:
    bool m_requiresSorting = false;

public:
    NODISCARD static RoomTexVector sortedRoomTexVector()
    {
        RoomTexVector v;
        v.m_requiresSorting = true;
        return v;
    }
    RoomTexVector() = default;

    // sorting stl iterators is slower than christmas with GLIBCXX_DEBUG,
    // so we'll use pointers instead. std::stable_sort isn't
    // necessary because everything's opaque, so we'll use
    // vanilla std::sort, which is N log N instead of N^2 log N.
    void sortByTexture()
    {
        assert(m_requiresSorting);
        if (size() < 2) {
            return;
        }

        if (isSorted()) {
            return;
        }

        RoomTex *const beg = data();
        RoomTex *const end = beg + size();
        // NOTE: comparison will be std::less<RoomTex>, which uses
        // operator<() if it exists.
        std::sort(beg, end, RoomTex::isPartitioned);
    }

    NODISCARD bool isSorted() const
    {
        if (!m_requiresSorting || size() < 2) {
            return true;
        }

        const RoomTex *const beg = data();
        const RoomTex *const end = beg + size();
        return std::is_sorted(beg, end, RoomTex::isPartitioned);
    }
};

struct NODISCARD ColoredRoomTexVector final : public std::vector<ColoredRoomTex>
{
    NODISCARD static bool isSorted() { return true; }
};

template<typename T, typename Callback>
static void foreach_texture(const T &textures, Callback &&callback)
{
    using U = utils::remove_cvref_t<decltype(textures.front())>;
    static_assert(std::is_same_v<U, RoomTex> or std::is_same_v<U, ColoredRoomTex>);

    assert(textures.isSorted());

    const auto size = textures.size();
    for (size_t beg = 0, next = size; beg < size; beg = next) {
        const auto &rtex = textures[beg];
        const auto textureId = rtex.pos.array;

        size_t end = beg + 1;
        for (; end < size; ++end) {
            if (textureId != textures[end].pos.array) {
                break;
            }
        }

        next = end;
        /* note: casting to avoid passing references to beg and end */
        callback(static_cast<size_t>(beg), static_cast<size_t>(end));
    }
}

NODISCARD static LayerMeshesIntermediate::Fn createMeshFn(MAYBE_UNUSED const std::string_view what,
                                                          const RoomTintVector &room_tints,
                                                          const MMTexArrayPosition texture)
{
    if (room_tints.empty()) {
        return {};
    }

    const size_t count = room_tints.size();
    std::vector<RoomQuadTexVert> verts;
    verts.reserve(count);

    for (auto &thisVert : room_tints) {
        const auto &pos = thisVert.coord;
        const auto v0 = pos.to_ivec3();
        verts.emplace_back(v0, texture.position, thisVert.color);
    }

    return [v = std::move(verts), texture](OpenGL &g) {
        return g.createRoomQuadTexBatch(v, texture.array);
    };
}

NODISCARD static RoomTintArray<LayerMeshesIntermediate::Fn> createMeshFns(
    MAYBE_UNUSED const std::string_view what,
    const RoomTintArray<RoomTintVector> &room_tints,
    const MMTexArrayPosition texture)
{
    RoomTintArray<LayerMeshesIntermediate::Fn> result;
    for (auto tint : ALL_ROOM_TINTS) {
        result[tint] = createMeshFn(what, room_tints[tint], texture);
    }
    return result;
}

NODISCARD static LayerMeshesIntermediate::FnVec createSortedTexturedMeshes(
    const std::string_view what, const RoomTexVector &textures)
{
    if (textures.empty()) {
        return {};
    }

    const size_t numUniqueTextures = std::invoke([&textures]() -> size_t {
        size_t texCount = 0;
        ::foreach_texture(textures,
                          [&texCount](size_t /* beg */, size_t /*end*/) -> void { ++texCount; });
        return texCount;
    });

    if constexpr (IS_DEBUG_BUILD) {
        if (numUniqueTextures > 1) {
            MMLOG() << what << " has " << numUniqueTextures << " unique textures\n";

            {
                auto &&os = MMLOG();
                for (auto &x : textures) {
                    os << x.pos.array.value() << " at " << x.pos.position << "\n";
                }
            }
        }
    }

    LayerMeshesIntermediate::FnVec tmp_meshes;
    tmp_meshes.reserve(numUniqueTextures);
    const auto lambda = [&tmp_meshes, &textures](const size_t beg, const size_t end) -> void {
        const RoomTex &rtex = textures[beg];
        const size_t count = end - beg;

        std::vector<RoomQuadTexVert> verts;
        verts.reserve(count);

        for (size_t i = beg; i < end; ++i) {
            const RoomTex &thisVert = textures[i];
            const auto &pos = thisVert.coord;
            const auto v0 = pos.to_ivec3();
            const auto z = thisVert.pos.position;
            verts.emplace_back(v0, z);
        }

        tmp_meshes.emplace_back([v = std::move(verts), t = rtex.pos.array](OpenGL &g) {
            return g.createRoomQuadTexBatch(v, t);
        });
    };

    ::foreach_texture(textures, lambda);
    assert(tmp_meshes.size() == numUniqueTextures);
    return tmp_meshes;
}

NODISCARD static LayerMeshesIntermediate::FnVec createSortedColoredTexturedMeshes(
    const std::string_view what, const ColoredRoomTexVector &textures)
{
    if (textures.empty()) {
        return {};
    }

    const size_t numUniqueTextures = std::invoke([&textures]() -> size_t {
        size_t texCount = 0;
        ::foreach_texture(textures,
                          [&texCount](size_t /* beg */, size_t /*end*/) -> void { ++texCount; });
        return texCount;
    });

    if constexpr (IS_DEBUG_BUILD) {
        if (numUniqueTextures > 1) {
            MMLOG() << what << " has " << numUniqueTextures << " unique textures\n";

            {
                auto &&os = MMLOG();
                for (auto &x : textures) {
                    os << x.pos.array.value() << " at " << x.pos.position << "\n";
                }
            }
        }
    }

    LayerMeshesIntermediate::FnVec tmp_meshes;
    tmp_meshes.reserve(numUniqueTextures);

    const auto lambda = [&tmp_meshes, &textures](const size_t beg, const size_t end) -> void {
        const ColoredRoomTex &rtex = textures[beg];
        const size_t count = end - beg;

        std::vector<RoomQuadTexVert> verts;
        verts.reserve(count);

        for (size_t i = beg; i < end; ++i) {
            const ColoredRoomTex &thisVert = textures[i];
            const auto &pos = thisVert.coord;
            const auto v0 = pos.to_ivec3();
            const auto color = thisVert.colorId;
            const auto z = thisVert.pos.position;
            verts.emplace_back(v0, z, color);
        }

        tmp_meshes.emplace_back([v = std::move(verts), t = rtex.pos.array](OpenGL &g) {
            return g.createRoomQuadTexBatch(v, t);
        });
    };

    ::foreach_texture(textures, lambda);
    assert(tmp_meshes.size() == numUniqueTextures);
    return tmp_meshes;
}

struct NODISCARD LayerBatchData final
{
    MMTexArrayPosition white_pixel;

    RoomTexVector roomTerrains;
    RoomTexVector forestTerrains;
    std::vector<TerrainSpriteVert> organicGroundBase;
    std::vector<TerrainSpriteVert> groundSprites;
    std::vector<TerrainSpriteVert> terrainSprites;
    std::vector<TerrainSpriteVert> terrainEdges;
    std::vector<TerrainSpriteVert> terrainWalls;
    std::vector<TerrainSpriteVert> roofCaps;
    std::vector<TerrainSpriteVert> doorSprites;
    std::vector<TerrainSpriteVert> warningSprites;
    std::vector<TerrainSpriteVert> wallShadows;
    std::vector<TerrainSpriteVert> wallGrime;
    MMTextureId spriteTexture = INVALID_MM_TEXTURE_ID;
    RoomTexVector roomTrails;
    RoomTexVector legacyTrails;
    RoomTexVector roomOverlays = RoomTexVector::sortedRoomTexVector();
    // REVISIT: Consider storing up/down door lines in a separate batch,
    // so they can be rendered thicker.
    ColoredRoomTexVector doors;
    ColoredRoomTexVector legacyDoorLines;
    ColoredRoomTexVector solidWallLines;
    ColoredRoomTexVector dottedWallLines;
    ColoredRoomTexVector roomUpDownExits;
    RoomTexVector streamIns;
    RoomTexVector streamOuts;
    RoomTintArray<RoomTintVector> roomTints;
    RoomTintVector roomLayerBoostQuads;

    explicit LayerBatchData(MMTexArrayPosition white_pixel_)
        : white_pixel{white_pixel_}
    {}

    ~LayerBatchData() = default;
    DEFAULT_MOVES_DELETE_COPIES(LayerBatchData);

    void sort()
    {
        DECL_TIMER(t, "sort");

        roomOverlays.sortByTexture();
        // Larger Y is north. Preserve this ordering across room boundaries and
        // texture variants; all sprites share one array and one draw call.
        std::sort(terrainSprites.begin(), terrainSprites.end(), [](const auto &a, const auto &b) {
            const auto &pa = a.anchorSizeLayer;
            const auto &pb = b.anchorSizeLayer;
            return std::tie(pa.y, pa.x, pa.z, a.textureColor.x)
                   > std::tie(pb.y, pb.x, pb.z, b.textureColor.x);
        });
    }

    NODISCARD LayerMeshesIntermediate buildIntermediate() const
    {
        DECL_TIMER(t2, "LayerBatchData::buildIntermediate");
        LayerMeshesIntermediate meshes;
        meshes.terrain = ::createSortedTexturedMeshes("terrain", roomTerrains);
        meshes.forestTerrain = ::createSortedTexturedMeshes("forestTerrain", forestTerrains);
        // Stable random painter order lets forest and field fringes intermingle.
        // Water is the bottom material even when room iteration order changes.
        auto bases = organicGroundBase;
        std::stable_sort(bases.begin(), bases.end(), [](const auto &a, const auto &b) {
            return (a.textureColor.w == terrain_materials::WATER_KIND)
                   > (b.textureColor.w == terrain_materials::WATER_KIND);
        });
        auto ground = groundSprites;
        std::sort(ground.begin(), ground.end(), [](const auto &a, const auto &b) {
            const bool aWater = (a.textureColor.w & 15) == terrain_materials::WATER_PATCH_KIND;
            const bool bWater = (b.textureColor.w & 15) == terrain_materials::WATER_PATCH_KIND;
            if (aWater != bWater)
                return aWater;
            return std::tie(a.textureColor.z, a.anchorSizeLayer.y, a.anchorSizeLayer.x)
                   < std::tie(b.textureColor.z, b.anchorSizeLayer.y, b.anchorSizeLayer.x);
        });
        if (!ground.empty()) {
            meshes.groundSprites = [v = std::move(ground), texture = spriteTexture](OpenGL &gl) {
                return gl.createTerrainSpriteBatch(v, texture);
            };
        }
        if (!organicGroundBase.empty()) {
            meshes.organicGroundBase = [v = std::move(bases), texture = spriteTexture](OpenGL &gl) {
                return gl.createTerrainSpriteBatch(v, texture);
            };
        }
        if (!terrainSprites.empty()) {
            meshes.terrainSprites = [v = terrainSprites, texture = spriteTexture](OpenGL &gl) {
                return gl.createTerrainSpriteBatch(v, texture);
            };
        }
        meshes.trails = ::createSortedTexturedMeshes("trails", roomTrails);
        meshes.legacyTrails = ::createSortedTexturedMeshes("legacyTrails", legacyTrails);
        if (!terrainEdges.empty()) {
            meshes.terrainEdges = [v = terrainEdges, texture = spriteTexture](OpenGL &gl) {
                return gl.createTerrainSpriteBatch(v, texture);
            };
        }
        if (!doorSprites.empty()) {
            meshes.doorSprites = [v = doorSprites, texture = spriteTexture](OpenGL &gl) {
                return gl.createTerrainSpriteBatch(v, texture);
            };
        }
        if (!warningSprites.empty()) {
            meshes.warningSprites = [v = warningSprites, texture = spriteTexture](OpenGL &gl) {
                return gl.createTerrainSpriteBatch(v, texture);
            };
        }
        if (!terrainWalls.empty()) {
            meshes.terrainWalls = [v = terrainWalls, texture = spriteTexture](OpenGL &gl) {
                return gl.createTerrainSpriteBatch(v, texture);
            };
        }
        if (!roofCaps.empty()) {
            meshes.roofCaps = [v = roofCaps, texture = spriteTexture](OpenGL &gl) {
                return gl.createTerrainSpriteBatch(v, texture);
            };
        }
        if (!wallShadows.empty()) {
            meshes.wallShadows = [v = wallShadows, texture = spriteTexture](OpenGL &gl) {
                return gl.createTerrainSpriteBatch(v, texture);
            };
        }
        if (!wallGrime.empty()) {
            meshes.wallGrime = [v = wallGrime, texture = spriteTexture](OpenGL &gl) {
                return gl.createTerrainSpriteBatch(v, texture);
            };
        }
        meshes.tints = ::createMeshFns("tints", roomTints, white_pixel);
        meshes.overlays = ::createSortedTexturedMeshes("overlays", roomOverlays);
        meshes.doors = ::createSortedColoredTexturedMeshes("doors", doors);
        meshes.legacyDoorLines = ::createSortedColoredTexturedMeshes("legacyDoorLines",
                                                                     legacyDoorLines);
        meshes.walls = ::createSortedColoredTexturedMeshes("solidWalls", solidWallLines);
        meshes.dottedWalls = ::createSortedColoredTexturedMeshes("dottedWalls", dottedWallLines);
        meshes.upDownExits = ::createSortedColoredTexturedMeshes("upDownExits", roomUpDownExits);
        meshes.streamIns = ::createSortedTexturedMeshes("streamIns", streamIns);
        meshes.streamOuts = ::createSortedTexturedMeshes("streamOuts", streamOuts);
        meshes.layerBoost = ::createMeshFn("layerBoost", roomLayerBoostQuads, white_pixel);
        meshes.isValid = true;
        return meshes;
    }

    NODISCARD LayerMeshes getMeshes(OpenGL &gl) const
    {
        DECL_TIMER(t, "LayerBatchData::getMeshes");
        return buildIntermediate().getLayerMeshes(gl);
    }
};

class NODISCARD LayerBatchBuilder final : public IRoomVisitorCallbacks
{
private:
    LayerBatchData &m_data;
    const mctp::MapCanvasTexturesProxy &m_textures;
    const OptBounds &m_bounds;
    const bool m_organicTerrain;
    std::unordered_map<Coordinate, terrain_edges::Sector> m_sectors;
    std::unordered_map<Coordinate, uint32_t> m_wallMasks;
    std::unordered_map<Coordinate, uint32_t> m_buildingWalls;
    std::unordered_map<Coordinate, const RoomHandle *> m_roomHandles;

    NODISCARD std::optional<terrain_materials::Family> wallFamily(const RoomTerrainEnum terrain,
                                                                  const Coordinate &pos,
                                                                  const ExitDirEnum dir) const
    {
        const auto across = m_sectors.find(pos + exitDir(dir));
        return terrain_walls::family(terrain,
                                     across != m_sectors.end()
                                         && across->second == terrain_edges::Sector::INDOORS,
                                     across != m_sectors.end()
                                         && across->second == terrain_edges::Sector::URBAN);
    }

    NODISCARD uint32_t wallMask(const Coordinate &pos) const
    {
        const auto own = m_wallMasks.find(pos);
        uint32_t mask = own == m_wallMasks.end() ? 0u : own->second;
        for (const auto dir : ALL_EXITS_NESW) {
            const auto next = m_wallMasks.find(pos + exitDir(dir));
            const auto index = static_cast<uint32_t>(dir);
            if (next != m_wallMasks.end() && (next->second & (1u << (index ^ 1u))))
                mask |= 1u << index;
        }
        return mask;
    }

    NODISCARD unsigned wallEnds(const RoomTerrainEnum terrain,
                                const Coordinate &pos,
                                const ExitDirEnum dir) const
    {
        const auto face = wallFamily(terrain, pos, dir);
        unsigned continuation = 0;
        for (const auto along : terrain_walls::alongDirections(static_cast<unsigned>(dir))) {
            const auto next = pos + exitDir(static_cast<ExitDirEnum>(along));
            const auto sector = m_sectors.find(next);
            const auto ownSector = m_sectors.find(pos);
            if ((wallMask(next) & (1u << static_cast<unsigned>(dir))) && sector != m_sectors.end()
                && ownSector != m_sectors.end() && sector->second == ownSector->second
                && wallFamily(terrain, next, dir) == face)
                continuation |= 1u << along;
        }
        return terrain_walls::ends(static_cast<unsigned>(dir), continuation);
    }

    NODISCARD uint32_t overlapMask(const Coordinate &pos) const
    {
        const auto walls = wallMask(pos);
        uint32_t allowed = (~walls) & 15u;
        // Both routes around a diagonal corner must be clear. This prevents
        // a large patch or canopy bypassing a wall just beyond its source room.
        for (uint32_t i = 0; i < 4; ++i) {
            const uint32_t vertical = i < 2 ? 0u : 1u;
            const uint32_t horizontal = (i & 1u) == 0 ? 2u : 3u;
            const auto v = static_cast<ExitDirEnum>(vertical);
            const auto h = static_cast<ExitDirEnum>(horizontal);
            if (!(walls & ((1u << vertical) | (1u << horizontal)))
                && !(wallMask(pos + exitDir(v)) & (1u << horizontal))
                && !(wallMask(pos + exitDir(h)) & (1u << vertical)))
                allowed |= 1u << (i + 4u);
        }
        return allowed;
    }

    NODISCARD static terrain_edges::Sector edgeSector(const RoomTerrainEnum terrain)
    {
        using S = terrain_edges::Sector;
        switch (terrain) {
        case RoomTerrainEnum::FIELD:
            return S::FIELD;
        case RoomTerrainEnum::FOREST:
            return S::FOREST;
        case RoomTerrainEnum::HILLS:
            return S::HILLS;
        case RoomTerrainEnum::MOUNTAINS:
            return S::MOUNTAINS;
        case RoomTerrainEnum::SHALLOW:
            return S::MARSH;
        case RoomTerrainEnum::WATER:
        case RoomTerrainEnum::RAPIDS:
            return S::WATER;
        case RoomTerrainEnum::UNDERWATER:
            return S::UNDERWATER;
        case RoomTerrainEnum::ROAD:
            return S::ROAD;
        case RoomTerrainEnum::CITY:
            return S::URBAN;
        case RoomTerrainEnum::INDOORS:
            return S::INDOORS;
        case RoomTerrainEnum::TUNNEL:
            return S::TUNNEL;
        case RoomTerrainEnum::CAVERN:
            return S::CAVERN;
        case RoomTerrainEnum::BRUSH:
            return S::BRUSH;
        default:
            return S::UNKNOWN;
        }
    }

    NODISCARD uint32_t routeMask(const RoomHandle &room) const
    {
        uint32_t mask = static_cast<uint32_t>(getRoadIndex(room.getRaw()));
        if (room.getTerrainType() != RoomTerrainEnum::ROAD)
            return mask;
        const auto walls = wallMask(room.getPosition());
        for (const auto dir : ALL_EXITS_NESW) {
            const auto side = static_cast<unsigned>(dir);
            const auto next = m_roomHandles.find(room.getPosition() + exitDir(dir));
            if ((walls & (1u << side)) || next == m_roomHandles.end())
                continue;
            const auto &peer = *next->second;
            if (peer.getTerrainType() == RoomTerrainEnum::ROAD
                || peer.getTerrainType() == RoomTerrainEnum::CITY)
                continue;
            const auto peerMask = static_cast<uint32_t>(getRoadIndex(peer.getRaw()));
            if (peerMask & (1u << (side ^ 1u)))
                mask |= 1u << side;
        }
        return mask;
    }

    void addWarning(const RoomHandle &room)
    {
        using F = terrain_materials::Family;
        if (!m_organicTerrain || !room.getLoadFlags().contains(RoomLoadFlagEnum::DEATHTRAP)
            || !m_textures.terrainWallPacks.at(static_cast<std::size_t>(F::DEATH_TRAP)))
            return;
        const auto tex = m_textures.terrain_sprites.at(static_cast<std::size_t>(
            terrain_materials::atlas(m_textures.terrainGroundPacks[0], F::DEATH_TRAP)));
        const auto pos = room.getPosition();
        TerrainSpriteVert warning;
        warning.anchorSizeLayer = {static_cast<float>(pos.x) + 0.5f,
                                   static_cast<float>(pos.y) + 0.5f,
                                   1.f,
                                   static_cast<float>(pos.z)};
        warning.textureColor = {tex.position * 16,
                                static_cast<int>(NamedColorEnum::DEFAULT),
                                0,
                                terrain_materials::WARNING_KIND | terrain_materials::ALL_SEASON
                                    | terrain_materials::NO_SKY_SHADOW};
        m_data.spriteTexture = tex.array;
        m_data.warningSprites.emplace_back(warning);
    }

    void addDoor(const RoomHandle &room, const ExitDirEnum dir)
    {
        using F = terrain_materials::Family;
        const auto side = static_cast<unsigned>(dir);
        const auto back = static_cast<ExitDirEnum>(side ^ 1u);
        const auto peerIt = m_roomHandles.find(room.getPosition() + exitDir(dir));
        const RoomHandle *peer = peerIt == m_roomHandles.end() ? nullptr : peerIt->second;
        const auto &exit = room.getExit(dir);
        const bool paired = peer && peer->getExit(back).getExitFlags().isDoor()
                            && exit.getOutgoingSet().contains(peer->getId())
                            && peer->getExit(back).getOutgoingSet().contains(room.getId());
        const auto id = room.getIdExternal().asUint32();
        const auto variant = terrain_doors::variant(id,
                                                    paired ? peer->getIdExternal().asUint32() : 0u,
                                                    side < 2u ? 0u : 1u);
        const auto tex = m_textures.terrain_sprites.at(static_cast<std::size_t>(
            terrain_materials::atlas(m_textures.terrainGroundPacks[0], F::DOOR)));
        const auto pos = room.getPosition();
        TerrainSpriteVert door;
        door.anchorSizeLayer = {static_cast<float>(pos.x) + 0.5f,
                                static_cast<float>(pos.y) + 0.5f,
                                1.f,
                                static_cast<float>(pos.z)};
        int flags = terrain_materials::WALL_KIND | terrain_materials::ALL_SEASON
                    | terrain_materials::NO_SKY_SHADOW | terrain_materials::DOOR_OVERLAY
                    | static_cast<int>(terrain_walls::rotation(side) << 4u);
        if (paired && (side == 1u || side == 3u))
            flags |= terrain_materials::DOOR_OTHER_SIDE;
        flags |= static_cast<int>(side << terrain_materials::DOOR_DIRECTION_SHIFT);
        // Static identity only. The current observation selects state in the
        // vertex shader, leaving this instance buffer valid across movement.
        door.textureColor = {tex.position * 16 + static_cast<int>(variant),
                             terrain_doors::roomWord(room.getServerId().asUint32()),
                             terrain_doors::roomWord(paired ? peer->getServerId().asUint32() : 0u),
                             flags};
        m_data.spriteTexture = tex.array;
        m_data.doorSprites.emplace_back(door);
    }

    NODISCARD bool hasOrganicRoute(const RoomHandle &room) const
    {
        const auto terrain = room.getTerrainType();
        return m_organicTerrain && routeMask(room) != 0 && terrain != RoomTerrainEnum::INDOORS
               && terrain != RoomTerrainEnum::CITY && terrain != RoomTerrainEnum::UNDERWATER;
    }

    void addTerrainEdges(const RoomHandle &room)
    {
        if (!m_organicTerrain)
            return;
        const auto terrain = room.getTerrainType();
        const bool route = hasOrganicRoute(room);
        if (!route && terrain != RoomTerrainEnum::ROAD)
            return;
        const auto pos = room.getPosition();
        terrain_edges::Neighbours neighbours{};
        for (const auto dir : ALL_EXITS_NESW) {
            const auto next = m_sectors.find(pos + exitDir(dir));
            if (next != m_sectors.end())
                neighbours[static_cast<size_t>(dir)] = next->second;
        }
        const auto tint = room.getLightType() == RoomLightEnum::DARK
                              ? getTintColor(RoomTintEnum::DARK)
                          : room.getSundeathType() == RoomSundeathEnum::NO_SUNDEATH
                              ? getTintColor(RoomTintEnum::NO_SUNDEATH)
                              : NamedColorEnum::DEFAULT;
        TerrainSpriteVert v;
        v.anchorSizeLayer = glm::vec4{static_cast<float>(pos.x) + 0.5f,
                                      static_cast<float>(pos.y) + 0.5f,
                                      1.f,
                                      static_cast<float>(pos.z)};
        const auto first = m_textures.terrain_sprites
                               .at(static_cast<std::size_t>(m_textures.terrainGroundPacks[0]))
                               .position
                           * 16;
        v.textureColor
            = glm::ivec4{static_cast<int>(
                             terrain_edges::withMaterials(terrain_edges::pack(edgeSector(terrain),
                                                                              neighbours),
                                                          static_cast<uint32_t>(first))),
                         static_cast<int>(tint),
                         static_cast<int>(terrain_edges::seed(room.getIdExternal().asUint32())
                                          | (wallMask(pos) << 24u)),
                         0};
        m_data.spriteTexture = m_textures.terrain_sprites.front().array;
        const bool road = terrain == RoomTerrainEnum::ROAD;
        const auto mask = routeMask(room);
        v.textureColor.w = terrain_edges::flags(road ? terrain_edges::ROAD_BASE
                                                     : terrain_edges::TRAIL,
                                                mask);
        if (terrain == RoomTerrainEnum::TUNNEL || terrain == RoomTerrainEnum::CAVERN)
            v.textureColor.w |= terrain_materials::ALL_SEASON;
        if (road) {
            // Composite road and undercoat before neighbouring ground/canopies.
            // The normal tint pass applies once to the complete base material.
            v.textureColor.y = static_cast<int>(NamedColorEnum::DEFAULT);
            m_data.organicGroundBase.emplace_back(v);
            uint32_t joins = 0;
            for (unsigned side = 0; side < 4; ++side) {
                const auto dir = static_cast<ExitDirEnum>(side);
                const auto peer = m_roomHandles.find(pos + exitDir(dir));
                if (!(mask & (1u << side)) || (wallMask(pos) & (1u << side))
                    || peer == m_roomHandles.end())
                    continue;
                if (peer->second->getTerrainType() != RoomTerrainEnum::ROAD
                    && peer->second->getTerrainType() != RoomTerrainEnum::CITY
                    && (static_cast<uint32_t>(getRoadIndex(peer->second->getRaw()))
                        & (1u << (side ^ 1u))))
                    joins |= 1u << side;
            }
            if (joins != 0) {
                auto apron = v;
                apron.textureColor.w = terrain_edges::flags(terrain_edges::ROAD, joins)
                                       | terrain_materials::ROUTE_APRON;
                m_data.terrainEdges.emplace_back(apron);
            }
        } else {
            m_data.terrainEdges.emplace_back(v);
        }
    }

public:
    explicit LayerBatchBuilder(LayerBatchData &data,
                               const mctp::MapCanvasTexturesProxy &textures,
                               const OptBounds &bounds,
                               const bool organicTerrain,
                               const RoomVector &rooms)
        : m_data{data}
        , m_textures{textures}
        , m_bounds{bounds}
        , m_organicTerrain{organicTerrain}
    {
        if (m_organicTerrain) {
            m_sectors.reserve(rooms.size());
            m_wallMasks.reserve(rooms.size());
            for (const auto &room : rooms) {
                m_roomHandles.try_emplace(room.getPosition(), &room);
                const auto [it, inserted] = m_sectors.emplace(room.getPosition(),
                                                              edgeSector(room.getTerrainType()));
                uint32_t walls = 0;
                for (const auto dir : ALL_EXITS_NESW) {
                    const auto flags = room.getExit(dir).getExitFlags();
                    if (!flags.isExit() || flags.isDoor())
                        walls |= 1u << static_cast<uint32_t>(dir);
                }
                if (room.getTerrainType() == RoomTerrainEnum::INDOORS
                    || room.getTerrainType() == RoomTerrainEnum::CITY) {
                    uint32_t solid = 0;
                    for (const auto dir : ALL_EXITS_NESW) {
                        const auto flags = room.getExit(dir).getExitFlags();
                        if (!flags.isExit() && !flags.isDoor())
                            solid |= 1u << static_cast<uint32_t>(dir);
                    }
                    m_buildingWalls[room.getPosition()] |= solid;
                }
                const auto [wall, wallInserted] = m_wallMasks.emplace(room.getPosition(), walls);
                if (!wallInserted)
                    wall->second = 15u;
                // Superimposed rooms are ambiguous; do not invent a bank there.
                if (!inserted)
                    it->second = terrain_edges::Sector::UNKNOWN;
            }
        }
    }

    ~LayerBatchBuilder() final;

    DELETE_CTORS_AND_ASSIGN_OPS(LayerBatchBuilder);

private:
    NODISCARD bool virt_acceptRoom(const RoomHandle &room) const final
    {
        return m_bounds.contains(room.getPosition());
    }

    void virt_visitTerrainTexture(const RoomHandle &room, const MMTexArrayPosition &terrain) final
    {
        // Hazard visibility must not depend on a legacy sector texture existing.
        addWarning(room);
        if (terrain.array == INVALID_MM_TEXTURE_ID
            && !(m_organicTerrain && room.getTerrainType() == RoomTerrainEnum::ROAD))
            return;
        using F = terrain_materials::Family;
        using S = terrain_edges::Sector;
        const auto pos = room.getPosition();
        const auto sector = edgeSector(room.getTerrainType());
        const bool underground = sector == S::TUNNEL || sector == S::CAVERN;
        const bool paved = sector == S::URBAN || sector == S::INDOORS;
        const bool underwater = sector == S::UNDERWATER;
        const int allSeason = (underground || paved || underwater) ? terrain_materials::ALL_SEASON
                                                                   : 0;
        const int shadowFlag = terrain_walls::receivesSkyShadows(room.getTerrainType(),
                                                                 room.getLightType(),
                                                                 room.getSundeathType())
                                   ? 0
                                   : terrain_materials::NO_SKY_SHADOW;
        const bool supported = underground || paved || underwater || sector == S::FIELD
                               || sector == S::FOREST || sector == S::HILLS
                               || sector == S::MOUNTAINS || sector == S::MARSH || sector == S::BRUSH
                               || (sector == S::WATER
                                   && room.getTerrainType() != RoomTerrainEnum::UNDERWATER)
                               || sector == S::ROAD;
        if (!m_organicTerrain || !supported) {
            m_data.roomTerrains.emplace_back(pos, terrain);
        } else {
            if (terrain.array != INVALID_MM_TEXTURE_ID)
                m_data.forestTerrains.emplace_back(pos, terrain); // Legacy/other-elevation fallback.
            const auto tint = room.getLightType() == RoomLightEnum::DARK
                                  ? getTintColor(RoomTintEnum::DARK)
                              : room.getSundeathType() == RoomSundeathEnum::NO_SUNDEATH
                                  ? getTintColor(RoomTintEnum::NO_SUNDEATH)
                                  : NamedColorEnum::DEFAULT;
            const auto id = room.getIdExternal().asUint32();
            const auto walls = wallMask(pos);
            const auto allowed = overlapMask(pos);
            const auto mask = static_cast<uint32_t>(getRoadIndex(room.getRaw()));
            const terrain_sprites::TrailSegments trail{bool(mask & 1u),
                                                       bool(mask & 2u),
                                                       bool(mask & 4u),
                                                       bool(mask & 8u)};
            const auto firstShared = m_textures.terrainGroundPacks[0];
            auto atlas = [&](F family) { return terrain_materials::atlas(firstShared, family); };
            m_data.spriteTexture = m_textures.terrain_sprites.front().array;
            static const std::array<Coordinate, 8> roadOffsets{Coordinate{0, 1, 0},
                                                               Coordinate{0, -1, 0},
                                                               Coordinate{1, 0, 0},
                                                               Coordinate{-1, 0, 0},
                                                               Coordinate{1, 1, 0},
                                                               Coordinate{-1, 1, 0},
                                                               Coordinate{1, -1, 0},
                                                               Coordinate{-1, -1, 0}};
            uint32_t roadNeighbours = 0;
            for (std::size_t i = 0; i < roadOffsets.size(); ++i) {
                const auto peer = m_sectors.find(pos + roadOffsets[i]);
                if (peer != m_sectors.end() && peer->second == S::ROAD)
                    roadNeighbours |= 1u << i;
            }
            auto add = [&](auto &batch,
                           const terrain_sprites::Sprite &sprite,
                           int pack,
                           int kind,
                           NamedColorEnum color) {
                const auto tex = m_textures.terrain_sprites.at(static_cast<std::size_t>(pack));
                TerrainSpriteVert v;
                v.anchorSizeLayer = glm::vec4{static_cast<float>(pos.x) + sprite.x,
                                              static_cast<float>(pos.y) + sprite.y,
                                              sprite.size,
                                              static_cast<float>(pos.z)};
                v.textureColor = glm::ivec4{tex.position * 16 + static_cast<int>(sprite.variant),
                                            static_cast<int>(color),
                                            static_cast<int>(sprite.transition | (allowed << 16u)
                                                             | (walls << 24u)),
                                            kind | shadowFlag};
                if ((kind & 15) == 2)
                    v.textureColor.w |= static_cast<int>(roadNeighbours << 20u);
                batch.emplace_back(v);
            };
            auto family = [](S value) {
                switch (value) {
                case S::FOREST:
                    return F::FOREST_GROUND;
                case S::HILLS:
                    return F::HILL_GROUND;
                case S::MOUNTAINS:
                    return F::MOUNTAIN_GROUND;
                case S::MARSH:
                    return F::MARSH_GROUND;
                case S::BRUSH:
                    return F::BRUSH_GROUND;
                case S::WATER:
                    return F::WATER;
                case S::UNDERWATER:
                    return F::UNDERWATER_GROUND;
                case S::TUNNEL:
                    return F::TUNNEL_GROUND;
                case S::CAVERN:
                    return F::CAVERN_GROUND;
                case S::URBAN:
                    return F::CITY_GROUND;
                case S::INDOORS:
                    return F::INDOOR_GROUND;
                default:
                    return F::FIELD_GROUND;
                }
            };
            if (sector != S::ROAD) {
                if (paved) {
                    add(m_data.organicGroundBase,
                        {0.5f, 0.5f, 1.f, 0, 32768},
                        atlas(family(sector)),
                        terrain_materials::PAVING_KIND | allSeason,
                        NamedColorEnum::DEFAULT);
                } else if (sector == S::WATER) {
                    add(m_data.organicGroundBase,
                        {0.5f, 0.5f, 1.f, 0, 32768},
                        atlas(F::WATER),
                        terrain_materials::WATER_KIND,
                        NamedColorEnum::DEFAULT);
                    // Allow patches across mapped water neighbours, including
                    // diagonals, but never beyond the actual water footprint.
                    static const std::array<Coordinate, 8> offsets{Coordinate{0, 1, 0},
                                                                   Coordinate{0, -1, 0},
                                                                   Coordinate{1, 0, 0},
                                                                   Coordinate{-1, 0, 0},
                                                                   Coordinate{1, 1, 0},
                                                                   Coordinate{-1, 1, 0},
                                                                   Coordinate{1, -1, 0},
                                                                   Coordinate{-1, -1, 0}};
                    uint32_t waterMask = 0;
                    for (size_t i = 0; i < offsets.size(); ++i) {
                        const auto it = m_sectors.find(pos + offsets[i]);
                        if (it != m_sectors.end() && it->second == S::WATER)
                            waterMask |= 1u << i;
                    }
                    for (const auto &sprite : terrain_sprites::water(id)) {
                        const int flip
                            = static_cast<int>(terrain_sprites::mix(sprite.transition) & 1u) << 8;
                        add(m_data.groundSprites,
                            sprite,
                            atlas(F::WATER),
                            terrain_materials::WATER_PATCH_KIND | flip,
                            tint);
                        m_data.groundSprites.back().textureColor.z = static_cast<int>(
                            sprite.transition | ((waterMask & allowed) << 16u) | (walls << 24u));
                    }
                } else {
                    add(m_data.organicGroundBase,
                        {0.5f, 0.5f, 1.f, 0, 32768},
                        0,
                        3 | allSeason,
                        NamedColorEnum::DEFAULT);
                    const auto firstTile = m_textures.terrain_sprites
                                               .at(static_cast<std::size_t>(firstShared))
                                               .position
                                           * 16;
                    m_data.organicGroundBase.back().textureColor.x = static_cast<int>(
                        terrain_edges::withMaterials(static_cast<uint32_t>(sector),
                                                     static_cast<uint32_t>(firstTile)));
                    for (auto sprite : terrain_sprites::ground(id)) {
                        if (underground)
                            sprite.variant = terrain_sprites::mix(sprite.transition) % 24u;
                        add(m_data.groundSprites,
                            sprite,
                            atlas(family(sector)),
                            terrain_sprites::flags(terrain_sprites::Kind::GROUND, 0u) | allSeason,
                            tint);
                    }
                }
            }
            if (walls != 0) {
                add(m_data.wallGrime,
                    {0.5f, 0.5f, 1.f, 0, 32768},
                    atlas(F::FIELD_GROUND),
                    terrain_materials::GRIME_KIND | terrain_materials::ALL_SEASON,
                    tint);
                uint32_t depths = 0;
                uint32_t openEnds = 0;
                for (const auto dir : ALL_EXITS_NESW) {
                    const auto face = wallFamily(room.getTerrainType(), pos, dir);
                    if (!face || !m_textures.terrainWallPacks.at(static_cast<size_t>(*face)))
                        continue;
                    const float depth = terrain_walls::depth(*face)
                                        + (room.getTerrainType() == RoomTerrainEnum::CITY
                                               ? terrain_walls::GUTTER_DEPTH
                                               : 0.f);
                    depths |= terrain_walls::depthByte(depth) << (static_cast<unsigned>(dir) * 8u);
                    openEnds |= terrain_walls::openEnds(static_cast<unsigned>(dir),
                                                        wallEnds(room.getTerrainType(), pos, dir),
                                                        walls)
                                << (static_cast<unsigned>(dir) * 2u);
                }
                m_data.wallGrime.back().textureColor.x = static_cast<int>(depths);
                // Kind 14 reuses ground-overlap bits for two open ends per edge.
                m_data.wallGrime.back().textureColor.z &= ~static_cast<int>(255u << 16u);
                m_data.wallGrime.back().textureColor.z |= static_cast<int>(openEnds << 16u);
            }
            if (shadowFlag == 0) {
                // Pack a 5x5 neighbourhood once per receiver. The shader
                // unions all projected walls, avoiding darker double shadows.
                std::array<uint32_t, 4> casters{};
                bool any = false;
                for (size_t i = 0; i < terrain_walls::shadowOffsets.size(); ++i) {
                    const auto &offset = terrain_walls::shadowOffsets[i];
                    const Coordinate caster{pos.x + offset[0], pos.y + offset[1], pos.z};
                    const auto it = m_buildingWalls.find(caster);
                    if (it == m_buildingWalls.end() || it->second == 0)
                        continue;
                    casters[i / 8] |= it->second << ((i % 8) * 4);
                    any = true;
                }
                if (any) {
                    add(m_data.wallShadows,
                        {0.5f, 0.5f, 1.f, 0, 0},
                        atlas(F::FIELD_GROUND),
                        terrain_materials::WALL_SHADOW_KIND,
                        NamedColorEnum::DEFAULT);
                    m_data.wallShadows.back().anchorSizeLayer.z = terrain_walls::WALL_HEIGHT_ROOMS;
                    m_data.wallShadows.back().textureColor
                        = glm::ivec4{static_cast<int>(casters[0]),
                                     static_cast<int>(casters[2]),
                                     static_cast<int>(casters[1]),
                                     terrain_materials::WALL_SHADOW_KIND
                                         | terrain_materials::ALL_SEASON
                                         | (static_cast<int>(casters[3]) << 4)};
                }
            }
            // A convex outside building corner belongs to the diagonal street
            // or field room. Finish that quadrant in its own room, rather than
            // extending either bordering wall beyond its owning square.
            for (unsigned vertical = 0; vertical < 2; ++vertical) {
                for (unsigned horizontal = 2; horizontal < 4; ++horizontal) {
                    const auto v = static_cast<ExitDirEnum>(vertical);
                    const auto h = static_cast<ExitDirEnum>(horizontal);
                    const auto diag = m_sectors.find(pos + exitDir(v) + exitDir(h));
                    const auto nextV = m_sectors.find(pos + exitDir(v));
                    const auto nextH = m_sectors.find(pos + exitDir(h));
                    if (diag == m_sectors.end() || nextV == m_sectors.end()
                        || nextH == m_sectors.end() || nextV->second != sector
                        || nextH->second != sector
                        || terrain_walls::isIndoors(room.getTerrainType()))
                        continue;
                    const bool indoor = diag->second == S::INDOORS;
                    const bool city = diag->second == S::URBAN;
                    const auto face = terrain_walls::family(room.getTerrainType(), indoor, city);
                    if ((!indoor && !city) || !face
                        || (*face != F::CITY_EAVE
                            && !m_textures.terrainWallPacks.at(static_cast<size_t>(F::WALL_FINISH)))
                        || !m_textures.terrainWallPacks.at(static_cast<size_t>(*face))
                        || !(wallMask(pos + exitDir(v)) & (1u << horizontal))
                        || !(wallMask(pos + exitDir(h)) & (1u << vertical)))
                        continue;
                    const unsigned turn = vertical == 0 ? (horizontal == 3 ? 0u : 3u)
                                                        : (horizontal == 3 ? 1u : 2u);
                    const auto hash = terrain_sprites::mix(
                        id ^ (vertical * 4u + horizontal) * 0x434150u);
                    int style = terrain_materials::WALL_KIND | terrain_materials::ALL_SEASON
                                | terrain_materials::NO_SKY_SHADOW
                                | terrain_materials::WALL_CORNER_ONLY | static_cast<int>(turn << 4u)
                                | static_cast<int>(
                                    terrain_walls::depthByte(terrain_walls::depth(*face))
                                    << terrain_materials::WALL_DEPTH_SHIFT);
                    style |= *face == F::CITY_EAVE ? (terrain_materials::SHINGLE_EAVE
                                                      | terrain_materials::CITY_GUTTER)
                                                   : terrain_materials::STONE_FACE;
                    add(m_data.terrainWalls,
                        {0.5f, 0.5f, 1.f, (*face == F::CITY_EAVE ? 6u : 18u) + hash % 6u, 0},
                        atlas(*face == F::CITY_EAVE ? F::CITY_EAVE : F::WALL_FINISH),
                        style,
                        tint);
                    if (*face == F::CITY_EAVE
                        && m_textures.terrainWallPacks.at(static_cast<size_t>(F::WALL_FINISH))) {
                        const auto finish = m_textures.terrain_sprites.at(
                            static_cast<size_t>(atlas(F::WALL_FINISH)));
                        auto cap = m_data.terrainWalls.back();
                        cap.textureColor.x = finish.position * 16 + static_cast<int>(hash % 3u);
                        cap.textureColor.w |= terrain_materials::ROOF_CAP_OVERLAY;
                        cap.textureColor.w &= ~terrain_materials::CITY_GUTTER;
                        cap.textureColor.z = 0;
                        m_data.roofCaps.emplace_back(cap);
                        // Split one shared cap into three room-owned fragments.
                        // The north/west shoulders overlap neighbouring roof
                        // strips without drawing into the diagonal indoor room.
                        for (unsigned fragment = 1; fragment <= 2; ++fragment) {
                            auto shoulder = cap;
                            const auto direction = (fragment == 1) == ((turn & 1u) == 0) ? v : h;
                            const auto offset = exitDir(direction);
                            shoulder.anchorSizeLayer.x += static_cast<float>(offset.x);
                            shoulder.anchorSizeLayer.y += static_cast<float>(offset.y);
                            shoulder.textureColor.z = static_cast<int>(fragment << 16u);
                            m_data.roofCaps.emplace_back(shoulder);
                        }
                    }
                }
            }
            // Road rooms use a composited base material; their own ground
            // patches would cover the route. Adjoining land fringes overlap it.
            if (sector != S::ROAD && sector != S::WATER && !paved) {
                auto sprites = sector == S::FIELD ? terrain_sprites::field(id)
                                                  : terrain_sprites::forest(id);
                F foreground = F::FIELD_DETAIL;
                size_t count = 9;
                float scale = 1.f;
                switch (sector) {
                case S::FOREST:
                    break;
                case S::TUNNEL:
                case S::CAVERN:
                    foreground = F::CAVE_DETAIL;
                    count = (terrain_sprites::mix(id ^ 0x424f4e45u) & 3u) == 0 ? 1 : 0;
                    scale = 0.55f;
                    break;
                case S::MOUNTAINS:
                    foreground = F::MOUNTAIN_PEAK;
                    count = 4;
                    scale = 1.85f;
                    break;
                case S::HILLS:
                    foreground = F::HILL;
                    count = 4;
                    scale = 1.9f;
                    break;
                case S::UNDERWATER:
                    foreground = F::UNDERWATER_DETAIL;
                    count = 4;
                    scale = 0.85f;
                    break;
                case S::MARSH:
                    foreground = F::REEDS;
                    count = 6;
                    scale = 0.8f;
                    break;
                case S::BRUSH:
                    foreground = F::BRUSH;
                    count = 5;
                    scale = 1.55f;
                    break;
                default:
                    break;
                }
                const auto regional = m_textures.terrainSpritePacks.find(
                    room.getArea().getStdStringViewUtf8());
                const int tree = regional == m_textures.terrainSpritePacks.end() ? 0
                                                                                 : regional->second;
                for (size_t i = 0; i < count; ++i) {
                    auto sprite = sprites[i];
                    if (terrain_sprites::blocksTree(sprite, trail))
                        continue;
                    if (sector == S::FIELD && (terrain_sprites::mix(sprite.transition) & 1u))
                        continue;
                    sprite.size *= scale;
                    if (sector == S::BRUSH)
                        sprite.size = std::min(sprite.size, 0.90f);
                    if (underwater)
                        sprite.variant = terrain_sprites::mix(sprite.transition ^ 0x534541u) % 12u;
                    const bool fitted = (sector == S::MOUNTAINS || sector == S::HILLS)
                                            ? terrain_sprites::fitForegroundScaled(sprite,
                                                                                   walls,
                                                                                   allowed)
                                            : terrain_sprites::fitForeground(sprite, walls, allowed);
                    if (!fitted || terrain_sprites::blocksTree(sprite, trail))
                        continue;
                    if (underground)
                        sprite.variant = terrain_sprites::mix(sprite.transition ^ 0x50524f50u)
                                         % 24u;
                    const int foregroundKind = sector == S::FOREST ? 0
                                               : sector == S::FIELD
                                                   ? 1
                                                   : terrain_materials::FOREGROUND_KIND;
                    add(m_data.terrainSprites,
                        sprite,
                        sector == S::FOREST ? tree : atlas(foreground),
                        foregroundKind | allSeason
                            | ((sector == S::MOUNTAINS || sector == S::HILLS || sector == S::BRUSH)
                                   ? terrain_materials::CASTS_SHADOW
                                   : 0)
                            | (sector == S::HILLS ? terrain_materials::MID_SHADOW : 0)
                            | (sector == S::BRUSH ? terrain_materials::LOW_SHADOW : 0),
                        tint);
                }
            }
        }
        addTerrainEdges(room);
        m_data.roomLayerBoostQuads.emplace_back(pos, NamedColorEnum::DEFAULT);
    }

    void virt_visitTrailTexture(const RoomHandle &room, const MMTexArrayPosition &trail) final
    {
        // Organic city paving is the route surface; do not paint a legacy
        // dirt trail over its cobbles. Keep legacy mode and other sectors.
        if (m_organicTerrain && room.getTerrainType() == RoomTerrainEnum::CITY)
            return;
        if (trail.array != INVALID_MM_TEXTURE_ID) {
            auto &batch = hasOrganicRoute(room) ? m_data.legacyTrails : m_data.roomTrails;
            batch.emplace_back(room.getPosition(), trail);
        }
    }

    void virt_visitOverlayTexture(const RoomHandle &room, const MMTexArrayPosition &overlay) final
    {
        if (overlay.array != INVALID_MM_TEXTURE_ID) {
            m_data.roomOverlays.emplace_back(room.getPosition(), overlay);
        }
    }

    void virt_visitNamedColorTint(const RoomHandle &room, const RoomTintEnum tint) final
    {
        const auto pos = room.getPosition();
        m_data.roomTints[tint].emplace_back(pos, getTintColor(tint));
    }

    void virt_visitWall(const RoomHandle &room,
                        const ExitDirEnum dir,
                        const NamedColorEnum color,
                        const WallTypeEnum wallType,
                        const bool isClimb) final
    {
        if (isTransparent(color)) {
            return;
        }

        if (m_organicTerrain && isNESW(dir) && room.getExit(dir).getExitFlags().isDoor()
            && m_textures.terrainWallPacks.at(
                static_cast<std::size_t>(terrain_materials::Family::DOOR))
            && (wallType == WallTypeEnum::DOOR || wallType == WallTypeEnum::SOLID)) {
            if (wallType == WallTypeEnum::DOOR)
                addDoor(room, dir);
            const auto tex = wallType == WallTypeEnum::DOOR ? m_textures.door[dir]
                                                            : m_textures.wall[dir];
            m_data.legacyDoorLines.emplace_back(room.getPosition(), tex, color);
            return; // Door pass supplies jamb stubs and the correctly open aperture.
        }
        if (wallType == WallTypeEnum::DOOR) {
            // Note: We could use two door textures (NESW and UD), and then just rotate the
            // texture coordinates, but doing that would require a different code path.
            const MMTexArrayPosition &tex = m_textures.door[dir];
            m_data.doors.emplace_back(room.getPosition(), tex, color);

        } else {
            if (isNESW(dir)) {
                if (wallType == WallTypeEnum::SOLID) {
                    const MMTexArrayPosition &tex = m_textures.wall[dir];
                    m_data.solidWallLines.emplace_back(room.getPosition(), tex, color);
                    const auto family = wallFamily(room.getTerrainType(), room.getPosition(), dir);
                    // Doors and unsupported environments retain MMapper's own
                    // renderer until appropriate overlay artwork is provided.
                    if (m_organicTerrain && family && !room.getExit(dir).getExitFlags().isDoor()) {
                        const auto slot = static_cast<size_t>(*family);
                        if (m_textures.terrainWallPacks.at(slot)) {
                            const auto pos = room.getPosition();
                            const auto hash = terrain_sprites::mix(
                                room.getIdExternal().asUint32()
                                ^ (static_cast<uint32_t>(dir) * 0x57414c4cu));
                            const auto pack
                                = terrain_materials::atlas(m_textures.terrainGroundPacks[0],
                                                           *family);
                            const auto image = m_textures.terrain_sprites.at(
                                static_cast<size_t>(pack));
                            const auto tint = room.getLightType() == RoomLightEnum::DARK
                                                  ? getTintColor(RoomTintEnum::DARK)
                                              : room.getSundeathType()
                                                      == RoomSundeathEnum::NO_SUNDEATH
                                                  ? getTintColor(RoomTintEnum::NO_SUNDEATH)
                                                  : NamedColorEnum::DEFAULT;
                            TerrainSpriteVert wall;
                            wall.anchorSizeLayer = glm::vec4{static_cast<float>(pos.x) + 0.5f,
                                                             static_cast<float>(pos.y) + 0.5f,
                                                             1.f,
                                                             static_cast<float>(pos.z)};
                            const bool roof = *family == terrain_materials::Family::CITY_EAVE;
                            // Adjacent rooms along one wall use the same
                            // periodic strip. Variety belongs within the art
                            // and between wall lines, not at every room join.
                            const auto variant = roof ? terrain_sprites::mix(
                                                            static_cast<uint32_t>(
                                                                dir == ExitDirEnum::NORTH
                                                                        || dir == ExitDirEnum::SOUTH
                                                                    ? pos.y
                                                                    : pos.x)
                                                            ^ (static_cast<uint32_t>(dir)
                                                               * 0x524f4f46u))
                                                            % 6u
                                                      : hash % 24u;
                            wall.textureColor
                                = glm::ivec4{image.position * 16 + static_cast<int>(variant),
                                             static_cast<int>(tint),
                                             static_cast<int>(hash & 65535u),
                                             terrain_materials::WALL_KIND
                                                 | terrain_materials::ALL_SEASON
                                                 | terrain_materials::NO_SKY_SHADOW
                                                 | (static_cast<int>(terrain_walls::rotation(
                                                        static_cast<unsigned>(dir)))
                                                    << 4)};
                            auto faceMask = wallMask(pos);
                            if (roof) {
                                // A stone perimeter beside a roof is not a
                                // valley. Only join perpendicular roof faces.
                                for (const auto side : ALL_EXITS_NESW)
                                    if (wallFamily(room.getTerrainType(), pos, side) != family)
                                        faceMask &= ~(1u << static_cast<unsigned>(side));
                            }
                            wall.textureColor.z |= static_cast<int>(faceMask << 24u);
                            wall.textureColor.w |= static_cast<int>(terrain_walls::depthByte(
                                                       terrain_walls::depth(*family)))
                                                   << terrain_materials::WALL_DEPTH_SHIFT;
                            if (*family == terrain_materials::Family::CITY_EAVE)
                                wall.textureColor.w |= terrain_materials::SHINGLE_EAVE;
                            if (room.getTerrainType() == RoomTerrainEnum::CITY)
                                wall.textureColor.w |= terrain_materials::CITY_GUTTER;
                            wall.textureColor.w
                                |= static_cast<int>(wallEnds(room.getTerrainType(), pos, dir))
                                   << terrain_materials::WALL_END_SHIFT;
                            if (*family == terrain_materials::Family::TIMBER_WALL)
                                wall.textureColor.w |= terrain_materials::TIMBER_FACE;
                            if (*family == terrain_materials::Family::STONE_WALL)
                                wall.textureColor.w |= terrain_materials::STONE_FACE;
                            if (m_textures.terrainWallPacks.at(
                                    static_cast<size_t>(terrain_materials::Family::WALL_FINISH))) {
                                const int finishPack = terrain_materials::atlas(
                                    m_textures.terrainGroundPacks[0],
                                    terrain_materials::Family::WALL_FINISH);
                                const auto finish = m_textures.terrain_sprites.at(
                                    static_cast<size_t>(finishPack));
                                wall.textureColor.z &= ~65535;
                                wall.textureColor.z |= finish.position * 16;
                                wall.textureColor.w |= terrain_materials::HAS_WALL_FINISH;
                            }
                            m_data.terrainWalls.emplace_back(wall);
                            if (roof
                                && (faceMask
                                    & (1u << terrain_walls::alongDirections(
                                           static_cast<unsigned>(dir))[0]))
                                && (wall.textureColor.w & terrain_materials::HAS_WALL_FINISH)) {
                                auto cap = wall;
                                cap.textureColor.x = (wall.textureColor.z & 65535) + 6
                                                     + static_cast<int>(variant % 3u);
                                cap.textureColor.z = 0;
                                cap.textureColor.w |= terrain_materials::ROOF_CAP_OVERLAY;
                                cap.textureColor.w &= ~terrain_materials::WALL_CORNER_ONLY;
                                m_data.roofCaps.emplace_back(cap);
                            }
                            m_data.spriteTexture = image.array;
                        }
                    }
                } else {
                    const MMTexArrayPosition &tex = m_textures.dotted_wall[dir];
                    m_data.dottedWallLines.emplace_back(room.getPosition(), tex, color);
                }
            } else {
                const bool isUp = dir == ExitDirEnum::UP;
                assert(isUp || dir == ExitDirEnum::DOWN);

                const MMTexArrayPosition &tex = isClimb ? (isUp ? m_textures.exit_climb_up
                                                                : m_textures.exit_climb_down)
                                                        : (isUp ? m_textures.exit_up
                                                                : m_textures.exit_down);

                m_data.roomUpDownExits.emplace_back(room.getPosition(), tex, color);
            }
        }
    }

    void virt_visitStream(const RoomHandle &room,
                          const ExitDirEnum dir,
                          const StreamTypeEnum type) final
    {
        switch (type) {
        case StreamTypeEnum::OutFlow:
            m_data.streamOuts.emplace_back(room.getPosition(), m_textures.stream_out[dir]);
            return;
        case StreamTypeEnum::InFlow:
            m_data.streamIns.emplace_back(room.getPosition(), m_textures.stream_in[dir]);
            return;
        default:
            break;
        }

        assert(false);
    }
};

LayerBatchBuilder::~LayerBatchBuilder() = default;

NODISCARD static LayerMeshesIntermediate generateLayerMeshes(
    const RoomVector &rooms,
    const mctp::MapCanvasTexturesProxy &textures,
    const OptBounds &bounds,
    const VisitRoomOptions &visitRoomOptions)
{
    DECL_TIMER(t, "generateLayerMeshes");

    LayerBatchData data{textures.white_pixel};
    LayerBatchBuilder builder{data, textures, bounds, visitRoomOptions.organicTerrain, rooms};
    visitRooms(rooms, textures, builder, visitRoomOptions);

    data.sort();
    return data.buildIntermediate();
}

struct NODISCARD InternalData final : public IMapBatchesFinisher
{
public:
    std::unordered_map<int, LayerMeshesIntermediate> batchedMeshes;
    BatchedConnections connectionDrawerBuffers;
    std::unordered_map<int, RoomNameBatchIntermediate> roomNameBatches;

private:
    void virt_finish(MapBatches &output, OpenGL &gl, GLFont &font) const final;
};

static void generateAllLayerMeshes(InternalData &internalData,
                                   const FontMetrics &font,
                                   const LayerToRooms &layerToRooms,
                                   const mctp::MapCanvasTexturesProxy &textures,
                                   const VisitRoomOptions &visitRoomOptions)

{
    // This feature has been removed, but it's passed to a lot of functions,
    // so it would be annoying to have to add it back if we decide the feature was
    // actually necessary.
    const OptBounds bounds{};

    DECL_TIMER(t, "generateAllLayerMeshes");
    auto &batchedMeshes = internalData.batchedMeshes;
    auto &connectionDrawerBuffers = internalData.connectionDrawerBuffers;
    auto &roomNameBatches = internalData.roomNameBatches;

    for (const auto &layer : layerToRooms) {
        DECL_TIMER(t2, "generateAllLayerMeshes.loop");
        const int thisLayer = layer.first;
        auto &layerMeshes = batchedMeshes[thisLayer];
        ConnectionDrawerBuffers &cdb = connectionDrawerBuffers[thisLayer];
        RoomNameBatch rnb;
        const auto &rooms = layer.second;

        {
            DECL_TIMER(t3, "generateAllLayerMeshes.loop.part2");
            layerMeshes = ::generateLayerMeshes(rooms, textures, bounds, visitRoomOptions);
        }

        {
            DECL_TIMER(t4, "generateAllLayerMeshes.loop.part3");

            // TODO: move everything in the same layer to the same internal struct?
            cdb.clear();
            rnb.clear();

            ConnectionDrawer cd{cdb, rnb, thisLayer, bounds};
            {
                DECL_TIMER(t7, "generateAllLayerMeshes.loop.part3b");
                // pass 2: add to buffers
                for (const auto &room : rooms) {
                    cd.drawRoomConnectionsAndDoors(room);
                }
            }
        }

        {
            DECL_TIMER(t8, "generateAllLayerMeshes.loop.part4");
            roomNameBatches[thisLayer] = rnb.getIntermediate(font);
        }
    }
}

LayerMeshes LayerMeshesIntermediate::getLayerMeshes(OpenGL &gl) const
{
    if (!isValid) {
        return {};
    }

    struct NODISCARD Resolver final
    {
        OpenGL &m_gl;

        NODISCARD UniqueMesh resolve(const Fn &fn)
        {
            if (!fn) {
                return {};
            }
            return fn(m_gl);
        }
        NODISCARD UniqueMeshVector resolve(const FnVec &v)
        {
            std::vector<UniqueMesh> result;
            result.reserve(v.size());
            for (const auto &f : v) {
                result.emplace_back(f(m_gl));
            }
            return UniqueMeshVector{std::move(result)};
        }
        NODISCARD RoomTintArray<UniqueMesh> resolve(const RoomTintArray<Fn> &arr)
        {
            RoomTintArray<UniqueMesh> result;
            for (const RoomTintEnum tint : ALL_ROOM_TINTS) {
                const Fn &fn = arr[tint];
                result[tint] = resolve(fn);
            }
            return result;
        }
    };

    DECL_TIMER(t, "LayerMeshesIntermediate::getLayerMeshes");
    Resolver r{gl};
    LayerMeshes lm;
    lm.terrain = r.resolve(terrain);
    lm.forestTerrain = r.resolve(forestTerrain);
    lm.organicGroundBase = r.resolve(organicGroundBase);
    lm.groundSprites = r.resolve(groundSprites);
    lm.terrainSprites = r.resolve(terrainSprites);
    lm.terrainEdges = r.resolve(terrainEdges);
    lm.terrainWalls = r.resolve(terrainWalls);
    lm.roofCaps = r.resolve(roofCaps);
    lm.doorSprites = r.resolve(doorSprites);
    lm.warningSprites = r.resolve(warningSprites);
    lm.wallShadows = r.resolve(wallShadows);
    lm.wallGrime = r.resolve(wallGrime);
    lm.legacyTrails = r.resolve(legacyTrails);
    lm.trails = r.resolve(trails);
    lm.tints = r.resolve(tints);
    lm.overlays = r.resolve(overlays);
    lm.doors = r.resolve(doors);
    lm.legacyDoorLines = r.resolve(legacyDoorLines);
    lm.walls = r.resolve(walls);
    lm.dottedWalls = r.resolve(dottedWalls);
    lm.upDownExits = r.resolve(upDownExits);
    lm.streamIns = r.resolve(streamIns);
    lm.streamOuts = r.resolve(streamOuts);
    lm.layerBoost = r.resolve(layerBoost);
    lm.isValid = true;

    return lm;
}

void LayerMeshes::render(const int thisLayer,
                         const int focusedLayer,
                         const bool drawTerrainSprites,
                         const glm::vec4 &shadow,
                         const float day,
                         const glm::ivec4 &doorState,
                         const GLRenderState &lighting)
{
    // Disable texturing for this layer. We want to draw
    // all of the squares in white (using layer boost quads),
    // and then still draw the walls.
    const bool disableTextures = (thisLayer > focusedLayer)
                                 && !getConfig().canvas.drawUpperLayersTextured;

    const bool organicSelected = drawTerrainSprites && thisLayer == focusedLayer;

    const GLRenderState defaultState = organicSelected ? lighting : GLRenderState{};
    const GLRenderState less = defaultState.withDepthFunction(DepthFunctionEnum::LESS);
    const GLRenderState equal = defaultState.withDepthFunction(DepthFunctionEnum::EQUAL);
    const GLRenderState lequal = defaultState.withDepthFunction(DepthFunctionEnum::LEQUAL);

    const GLRenderState less_blended = less.withBlend(BlendModeEnum::TRANSPARENCY);
    const GLRenderState lequal_blended = lequal.withBlend(BlendModeEnum::TRANSPARENCY);
    const GLRenderState equal_blended = equal.withBlend(BlendModeEnum::TRANSPARENCY);
    const GLRenderState equal_multiplied = equal.withBlend(BlendModeEnum::MODULATE);

    const auto color = (thisLayer <= focusedLayer) ? Colors::white.withAlpha(0.90f)
                                                   : Colors::gray70.withAlpha(0.20f);

    {
        /* REVISIT: For the modern case, we could render each layer separately,
         * and then only blend the layers that actually overlap. Doing that would
         * give higher contrast for the base textures.
         */
        if (disableTextures) {
            const auto layerWhite = Colors::white.withAlpha(0.20f);
            layerBoost.render(less_blended.withColor(layerWhite));
        } else {
            terrain.render(less_blended.withColor(color));
            if (organicSelected) {
                if (organicGroundBase)
                    organicGroundBase.render(less_blended.withColor(color).withTerrainDay(day));
            } else {
                forestTerrain.render(less_blended.withColor(color));
            }
        }
    }

    // REVISIT: move trails to their own batch also colored by the tint?
    for (const RoomTintEnum tint : ALL_ROOM_TINTS) {
        static_assert(NUM_ROOM_TINTS == 2);
        if (const UniqueMesh &mesh = tints[tint]) {
            auto tintState = equal_multiplied;
            tintState.uniforms.roomLightTintPass = 1;
            mesh.render(tintState);
        }
    }

    if (!disableTextures && organicSelected) {
        // Painter ordering handles transparency. With depth testing disabled,
        // OpenGL also leaves the depth buffer untouched: canopy overhangs are
        // not clipped to ground quads and cannot mask subsequent wall overlays.
        // Other elevations retain the existing terrain and fading in this pilot.
        const auto spriteState = defaultState.withBlend(BlendModeEnum::TRANSPARENCY)
                                     .withColor(color)
                                     .withTerrainDay(day);
        if (groundSprites)
            groundSprites.render(spriteState);
        if (wallGrime)
            wallGrime.render(spriteState);
        if (wallShadows && shadow.z > 0.f)
            wallShadows.render(spriteState.withTerrainShadow(shadow));
        if (terrainSprites && shadow.z > 0.f) {
            // Same static instance buffer and alpha images; one extra draw call.
            terrainSprites.render(spriteState.withTerrainShadow(shadow));
        }
        if (terrainSprites)
            terrainSprites.render(
                spriteState.withTerrainShadow(glm::vec4{shadow.x, shadow.y, shadow.z, 0.f}));
    }

    if (!disableTextures) {
        // streams go under everything else, including trails
        {
            const Color streamColor = XNamedColor{NamedColorEnum::STREAM}.getColor();
            const auto combined = Color::multiplyAsVec4(streamColor, color);
            streamIns.render(lequal_blended.withColor(combined));
            streamOuts.render(lequal_blended.withColor(combined));
        }

        trails.render(equal_blended.withColor(color));
        if (organicSelected) {
            if (terrainEdges)
                terrainEdges.render(defaultState.withBlend(BlendModeEnum::TRANSPARENCY)
                                        .withColor(color)
                                        .withTerrainDay(day));
        } else {
            legacyTrails.render(equal_blended.withColor(color));
        }
        overlays.render(equal_blended.withColor(color));
    }

    // always
    {
        // doors and walls are considered lines, even though they're drawn with textures.
        upDownExits.render(equal_blended.withColor(color));

        // Doors are drawn on top of the up-down exits
        doors.render(lequal_blended.withColor(color));
        if (!organicSelected || disableTextures)
            legacyDoorLines.render(lequal_blended.withColor(color));
        // and walls are drawn on top of doors.
        walls.render(lequal_blended.withColor(color));
        dottedWalls.render(lequal_blended.withColor(color));
        if (!disableTextures && organicSelected && terrainWalls) {
            // Opaque boundary texels cover the underlying wall; the full quad
            // remains inside its owning room, including at joined corners.
            terrainWalls.render(defaultState.withBlend(BlendModeEnum::TRANSPARENCY)
                                    .withColor(Colors::white)
                                    .withTerrainDay(day));
        }
        if (!disableTextures && organicSelected && roofCaps)
            roofCaps.render(defaultState.withBlend(BlendModeEnum::TRANSPARENCY)
                                .withColor(Colors::white)
                                .withTerrainDay(day));
    }

    // Door art is a final room-owned overlay, above legacy wall/door lines
    // and decorative roofs. Up/down doors keep their existing representation.
    if (!disableTextures && organicSelected && doorSprites)
        doorSprites.render(defaultState.withBlend(BlendModeEnum::TRANSPARENCY)
                               .withColor(Colors::white)
                               .withTerrainDay(day)
                               .withTerrainDoors(doorState));

    // Safety markings must survive legacy load/mob overlays, foregrounds
    // and wall decoration. Keep this above every map-room artwork pass.
    if (!disableTextures && organicSelected && warningSprites)
        warningSprites.render(defaultState.withBlend(BlendModeEnum::TRANSPARENCY)
                                  .withColor(Colors::white)
                                  .withTerrainDay(day));

    if (thisLayer != focusedLayer) {
        // Darker when below, lighter when above
        const auto baseAlpha = (thisLayer < focusedLayer) ? 0.5f : 0.1f;
        const auto alpha
            = glm::clamp(baseAlpha + 0.03f * static_cast<float>(std::abs(focusedLayer - thisLayer)),
                         0.f,
                         1.f);
        const Color &baseColor = (thisLayer < focusedLayer || disableTextures) ? Colors::black
                                                                               : Colors::white;
        layerBoost.render(equal_blended.withColor(baseColor.withAlpha(alpha)));
    }
}

void InternalData::virt_finish(MapBatches &output, OpenGL &gl, GLFont &font) const
{
    DECL_TIMER(t, "InternalData::virt_finish");

    {
        DECL_TIMER(t2, "InternalData::virt_finish batchedMeshes");
        for (const auto &kv : batchedMeshes) {
            const LayerMeshesIntermediate &data = kv.second;
            output.batchedMeshes[kv.first] = data.getLayerMeshes(gl);
        }
    }
    {
        DECL_TIMER(t2, "InternalData::virt_finish connectionMeshes");
        for (const auto &kv : connectionDrawerBuffers) {
            const ConnectionDrawerBuffers &data = kv.second;
            output.connectionMeshes[kv.first] = data.getMeshes(gl);
        }
    }

    {
        DECL_TIMER(t2, "InternalData::virt_finish roomNameBatches");
        for (const auto &kv : roomNameBatches) {
            const RoomNameBatchIntermediate &rnb = kv.second;
            output.roomNameBatches[kv.first] = rnb.getMesh(font);
        }
    }
}

// NOTE: All of the lamda captures are copied, including the texture data!
FutureSharedMapBatchFinisher generateMapDataFinisher(const mctp::MapCanvasTexturesProxy &textures,
                                                     const std::shared_ptr<const FontMetrics> &font,
                                                     const Map &map)
{
    const auto visitRoomOptions = getVisitRoomOptions();

    return std::async(std::launch::async,
                      [textures, font, map, visitRoomOptions]() -> SharedMapBatchFinisher {
                          ThreadLocalNamedColorRaii tlRaii{visitRoomOptions.canvasColors,
                                                           visitRoomOptions.colorSettings};
                          DECL_TIMER(t, "[ASYNC] generateAllLayerMeshes");

                          ProgressCounter dummyPc;
                          map.checkConsistency(dummyPc);

                          const auto layerToRooms = std::invoke([map]() -> LayerToRooms {
                              DECL_TIMER(t2, "[ASYNC] generateBatches.layerToRooms");
                              LayerToRooms ltr;
                              map.getRooms().for_each([&map, &ltr](const RoomId id) {
                                  const auto &r = map.getRoomHandle(id);
                                  const auto z = r.getPosition().z;
                                  auto &layer = ltr[z];
                                  layer.emplace_back(r);
                              });
                              return ltr;
                          });

                          auto result = std::make_shared<InternalData>();
                          auto &data = deref(result);
                          generateAllLayerMeshes(data,
                                                 deref(font),
                                                 layerToRooms,
                                                 textures,
                                                 visitRoomOptions);
                          return SharedMapBatchFinisher{result};
                      });
}

void finish(const IMapBatchesFinisher &finisher,
            std::optional<MapBatches> &opt_batches,
            OpenGL &gl,
            GLFont &font)
{
    MapBatches &batches = opt_batches.emplace();

    // Note: This will call InternalData::finish;
    // if necessary for claritiy, we could replace this with Pimpl to make it a direct call,
    // but that won't change the cost of the virtual call.
    finisher.finish(batches, gl, font);
}
