#pragma once
// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 The MMapper Authors
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
int appendSeasonalSheet(SpritePacks &packs, const QImage &sheet);
SpritePacks loadSpritePacks(const QString &manifest, const QImage &bundled);
} // namespace terrain_sprites
