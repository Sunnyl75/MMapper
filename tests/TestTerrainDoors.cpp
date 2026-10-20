// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 The MMapper Authors
#include "display/TerrainDoors.h"

#include <cassert>
#include <iostream>

#include <QJsonDocument>
#include <QJsonObject>

int main()
{
    using namespace terrain_doors;
    Observation seen;
    const auto json = [](const char *text) { return QJsonDocument::fromJson(text).object(); };
    assert(seen.observe(
        json(
            R"({"id":101,"exits":{"n":{"name":"gate","flags":["closed"]},"e":{"name":"door","flags":[]}}})"),
        false));
    assert(seen.serverRoom == 101 && seen.states[0] == State::CLOSED
           && seen.states[2] == State::OPEN);
    assert(seen.states[1] == State::UNKNOWN);
    assert(!seen.observe(json(R"({"id":202,"exits":{"n":{"flags":[]}}})"), true));
    assert(!seen.observe(json(R"({"exits":{"n":{"name":"gate"}}})"), true));
    assert(seen.states[0] == State::CLOSED);
    assert(seen.observe(json(R"({"exits":{"n":{"flags":[]}}})"), true));
    assert(seen.states[0] == State::OPEN);
    assert(seen.observe(json(R"({"w":{"flags":["closed"]}})"), true));
    assert(seen.states[3] == State::CLOSED);
    assert(seen.observe(json(R"({"id":202,"exits":{"s":{"flags":["closed"]}}})"), false));
    assert(seen.states[0] == State::UNKNOWN && seen.states[1] == State::CLOSED);
    seen.clear();
    assert(seen.serverRoom == 0 && seen.states[1] == State::UNKNOWN);
    assert(!seen.observe(json(R"({"exits":{"n":{"flags":[]}}})"), true));
    assert(seen.renderState() == (std::array<int32_t, 4>{0, 0, 0, 0}));
    assert(seen.observe(
        json(
            R"({"id":4294967295,"exits":{"n":{"flags":["closed"]},"e":{"flags":[]},"w":{"flags":["closed"]}}})"),
        false));
    const auto words = seen.renderState();
    assert(words[0] == roomWord(0xffffffffu));
    assert(words[1] == (1 | (2 << 4) | (1 << 6)));
    seen.clear();
    assert(seen.renderState() == (std::array<int32_t, 4>{0, 0, 0, 0}));
    for (uint32_t a = 1; a < 500; ++a)
        for (unsigned axis = 0; axis < 2; ++axis) {
            assert(variant(a, a + 1, axis) == variant(a + 1, a, axis));
            assert(variant(a, 0, axis) < 6);
        }
    std::cout
        << "Observed door state, remote/unknown handling and symmetric saved-ID variants passed\n";
}
