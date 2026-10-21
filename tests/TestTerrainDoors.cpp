// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 The MMapper Authors
#include "display/TerrainDoors.h"

#include <cassert>
#include <iostream>
#include <set>
int main()
{
    using terrain_doors::variant;
    std::set<uint32_t> seen;
    for (uint32_t a = 1; a < 500; ++a)
        for (unsigned axis = 0; axis < 2; ++axis) {
            assert(variant(a, a + 1, axis) == variant(a + 1, a, axis));
            assert(variant(a, 0, axis) < 6);
            seen.insert(variant(a, a + 1, axis));
        }
    assert(seen.size() == 6);
    assert(variant(0xffffffffu, 123u, 0u) == variant(123u, 0xffffffffu, 0u));
    std::cout << "Static door variants match across paired halves and cover all six artworks\n";
}
