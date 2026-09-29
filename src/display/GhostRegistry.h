#pragma once

#include "../group/CGroupChar.h"
#include "../map/roomid.h" // ServerRoomId

#include <unordered_map>

#include <QString>

struct GhostInfo
{
    ServerRoomId serverId;
    RoomId originRoomId;
    QString tokenKey;
    int framesAlive = 0;
};

extern std::unordered_map<ServerRoomId, GhostInfo> g_ghosts;
