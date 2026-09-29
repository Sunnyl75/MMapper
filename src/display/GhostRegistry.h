#pragma once

#include "../map/roomid.h" // ServerRoomId

#include <unordered_map>

#include <QString>

struct GhostInfo
{
    ServerRoomId serverId;
    RoomId originRoomId;
    QString tokenKey;
};

extern std::unordered_map<ServerRoomId, GhostInfo> g_ghosts;
