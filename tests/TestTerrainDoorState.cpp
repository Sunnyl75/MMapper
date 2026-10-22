// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 The MMapper Authors
#include "display/TerrainDoorState.h"
#include "proxy/GmcpMessage.h"

#include <cassert>
#include <chrono>
#include <iostream>
#include <tuple>

int main()
{
    using terrain_doors::Observation;
    using terrain_doors::State;
    const auto message = [](const char *bytes) {
        return GmcpMessage::fromRawBytes(QByteArray(bytes));
    };
    Observation doors;
    assert(!doors.appliesTo(0));
    assert(!doors.observe(message(R"(Room.UpdateExits {"n":{"name":"door"}})")));
    // Replay the actual captured server sequence, including an open update
    // with NO flags member, followed by the server closing the door again.
    const auto arrival = message(
        R"(Room.Info {"id":7266861,"exits":{"n":{"id":8682859,"name":"door"},"s":{"id":1699739}}})");
    assert(doors.observe(arrival));
    assert(!doors.observe(arrival));
    assert(doors.appliesTo(7266861));
    assert(!doors.appliesTo(6527585)); // Old mapped room while path matching catches up.
    assert(doors.state(0) == State::OPEN);
    assert(doors.state(1) == State::OPEN);
    assert(doors.observe(message(R"(Room.UpdateExits {"n":{"flags":["closed"],"name":"door"}})")));
    assert(doors.state(0) == State::CLOSED);
    assert(doors.state(1) == State::OPEN); // Omitted directions retain their states.
    assert(doors.observe(message(R"(Room.UpdateExits {"n":{"id":8682859,"name":"door"}})")));
    assert((doors.openMask() & 1u) != 0);
    assert(doors.observe(message(R"(Room.UpdateExits {"n":{"flags":["closed"],"name":"door"}})")));
    assert((doors.openMask() & 1u) == 0);
    assert(!doors.observe(message(R"(Room.UpdateExits {"n":{"flags":["closed"],"name":"door"}})")));
    assert(doors.observe(message(R"(room.updateexits {"e":{"name":"gate"}})")));
    assert(doors.state(2) == State::OPEN);
    assert(doors.observe(message(R"(Room.Update.Exits {"e":{"flags":["closed"]}})")));
    assert(doors.state(2) == State::CLOSED);
    const auto unchanged = doors;
    for (const auto *raw : {R"(Room.UpdateExits {})",
                            R"(Room.UpdateExits [])",
                            R"(Room.UpdateExits {"n":null})",
                            R"(Room.UpdateExits {"e":{"flags":false}})",
                            R"(Char.Vitals {"hp":100})",
                            "Room.UpdateExits"})
        assert(!doors.observe(message(raw)));
    assert(doors == unchanged);
    assert(doors.observe(message(R"(Room.UpdateExits {"s":false})")));
    assert(doors.state(1) == State::UNKNOWN);
    assert(doors.observe(message(R"(Room.UpdateExits {"n":{"flags":["broken"]}})")));
    assert(doors.state(0) == State::OPEN);
    assert(doors.observe(message(R"(Event.Moved {"dir":"west"})")));
    assert(doors.openMask() == 0);
    assert(!doors.appliesTo(7266861));
    assert(!doors.observe(message(R"(Room.UpdateExits {"n":{"name":"door"}})")));
    assert(doors.observe(message(R"(Room.Info {"id":4294967295,"exits":{"w":{"name":"door"}}})")));
    assert(doors.appliesTo(0xffffffffu));
    assert(doors.openMask() == 8u);
    assert(doors.observe(message("Core.Goodbye")));
    assert(!doors.clear());
    for (const auto *raw : {R"(Room.Info {"id":0})",
                            R"(Room.Info {"id":-1})",
                            R"(Room.Info {"id":12.5})",
                            R"(Room.Info {"id":4294967296})",
                            R"(Room.Info {"exits":{"n":{"name":"door"}}})"}) {
        std::ignore = doors.observe(message(raw));
        assert(!doors.appliesTo(0));
        assert(doors.openMask() == 0);
    }
    std::ignore = doors.observe(arrival);
    const auto opened = message(R"(Room.UpdateExits {"n":{"id":8682859,"name":"door"}})");
    const auto closed = message(R"(Room.UpdateExits {"n":{"flags":["closed"],"name":"door"}})");
    const auto started = std::chrono::steady_clock::now();
    for (unsigned i = 0; i < 10000; ++i) {
        std::ignore = doors.observe(closed);
        std::ignore = doors.observe(opened);
    }
    const auto elapsed = std::chrono::duration<double, std::micro>(std::chrono::steady_clock::now()
                                                                   - started)
                             .count();
    std::cout << "Door observation tests passed; CPU microseconds per exit update: "
              << elapsed / 20000.0 << '\n';
}
