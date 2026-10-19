#pragma once
// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 The MMapper Authors
#include "TerrainMaterials.h"

#include <map>
#include <string>
#include <vector>

#include <QImage>
#include <QString>
namespace terrain_sprites {
struct SpritePacks final
{
    std::vector<std::vector<QImage>> atlases;
    std::map<std::string, int, std::less<>> areas;
};
int appendRepeatMaterial(SpritePacks &packs, const QImage &source);
int appendFixedSheet(SpritePacks &packs, const QImage &sheet);
int appendSeasonalSheet(SpritePacks &packs, const QImage &sheet);
QImage alignDoorSheet(const QImage &source);
QImage expandFixedSheet(const QImage &source, int columns, int rows);
QImage alignSeasonalSheet(const QImage &sheet, terrain_materials::Placement placement);
void applyRoofCaps(QImage &finishes, const QImage &caps);
SpritePacks loadSpritePacks(const QString &manifest, const QImage &bundled);
} // namespace terrain_sprites
