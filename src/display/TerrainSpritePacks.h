#pragma once
// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 The MMapper Authors
#include "TerrainMaterials.h"

#include <array>
#include <functional>
#include <map>
#include <string>
#include <string_view>
#include <vector>

#include <QImage>
#include <QString>
#include <QStringList>
namespace terrain_sprites {
struct SpritePacks final
{
    std::vector<std::vector<QImage>> atlases;
    std::map<std::string, int, std::less<>> areas;
    std::map<std::string, int, std::less<>> namedAreas; // Media-style area keys.
    std::map<int, QStringList> directories;             // Ordered roots for each distinct pack.
};
struct PackSelection final
{
    int trees = 0;
    // Optional fixed-role details, absent by default. Shared by any area pack.
    int cartTracks = -1;
    int cityWallFringe = -1;
    std::array<int, terrain_materials::SHEETS.size()> materials{};
    std::array<bool, terrain_materials::SHEETS.size()> available{};
    int atlas(terrain_materials::Family family) const;
    bool has(terrain_materials::Family family) const;
};
struct PackSelections final
{
    std::vector<PackSelection> packs; // Default first; aliases share one selection.
    std::map<std::string, int, std::less<>> areas;
    std::map<std::string, int, std::less<>> namedAreas;
    int index(std::string_view area) const;
    const PackSelection &find(std::string_view area) const;
};
using ImageLoader = std::function<QImage(const QString &)>;
PackSelections loadMaterialPacks(SpritePacks &sprites,
                                 const ImageLoader &loadGlobal,
                                 int maxLayers = 256);
// Row 0..44: family colour bases and regional underpaint colours. Row 64+:
// colour-layer -> normal/reflectivity base and profile. Level-zero texel reads.
std::vector<QImage> packLookup(const SpritePacks &sprites,
                               const PackSelections &selections,
                               const std::vector<std::array<int, 3>> &reflectionSources);
int appendRepeatMaterial(SpritePacks &packs, const QImage &source);
int appendFixedSheet(SpritePacks &packs, const QImage &sheet);
int appendSeasonalSheet(SpritePacks &packs, const QImage &sheet);
QImage alignDoorSheet(const QImage &source);
QImage alignRoadShoreSheet(const QImage &source);
QImage expandFixedSheet(const QImage &source, int columns, int rows);
QImage alignSeasonalSheet(const QImage &sheet, terrain_materials::Placement placement);
void applyRoofCaps(QImage &finishes, const QImage &caps);
SpritePacks loadSpritePacks(const QString &manifest,
                            const QImage &bundled,
                            const QStringList &namedRoots = {});
} // namespace terrain_sprites
