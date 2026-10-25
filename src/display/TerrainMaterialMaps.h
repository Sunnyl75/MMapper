#pragma once
// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 The MMapper Authors

#include "TerrainMaterials.h"

#include <array>
#include <vector>

#include <QImage>
#include <QString>

namespace terrain_material_maps {
// Atlas pixels have OpenGL UV orientation already. RGB = unit normal encoded
// in [0,255], A = reflectivity DATA, never coverage or premultiplied colour.
enum class Surface { WATER, SHALLOWS, RAPIDS, MOUNTAIN };
struct Material final
{
    terrain_materials::Family family;
    Surface surface;
};
// Order is shared by the generated data pages and shader profile offsets.
inline constexpr std::array<Material, 4> MATERIALS{{
    {terrain_materials::Family::WATER, Surface::WATER},
    {terrain_materials::Family::MARSH_GROUND, Surface::SHALLOWS},
    {terrain_materials::Family::RAPIDS, Surface::RAPIDS},
    {terrain_materials::Family::MOUNTAIN_PEAK, Surface::MOUNTAIN},
}};
using Pages = std::vector<std::vector<QImage>>;
QImage generate(const QImage &source, Surface surface = Surface::WATER);
Pages neutralPages();
Pages loadOrGenerate(const Pages &colour,
                     const QString &cacheDirectory,
                     Surface surface = Surface::WATER);
} // namespace terrain_material_maps
