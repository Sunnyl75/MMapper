// SPDX-License-Identifier: GPL-2.0-or-later
#include "display/TerrainLightMask.h"
#include "display/TerrainRoomLightRules.h"

#include <cassert>
#include <iostream>
#include <set>

#include <QJsonDocument>

int main()
{
    const terrain_room_light::MaskAtlas atlas(
        QImage(QStringLiteral(TERRAIN_ASSET_DIR "/terrain-light-masks.png")));
    const auto steady = atlas.tile(42, 0., 100, 0);
    assert(steady == atlas.tile(42, 100., 100, 0));
    assert(steady != atlas.tile(43, 0., 100, 0));
    assert(atlas.tile(42, 0., 100, 100) != atlas.tile(42, .4, 100, 100));
    assert(atlas.tile(42, .4, 100, 100) != atlas.tile(43, .4, 100, 100));
    const auto off = atlas.tile(42, 1., 0, 100);
    for (int y = 0; y < off.height(); ++y)
        for (int x = 0; x < off.width(); ++x) {
            assert(qAlpha(off.pixel(x, y)) == 0);
            assert(qBlue(off.pixel(x, y)) == 0);
        }
    for (int frame = 0; frame < 48; ++frame) {
        const auto tile = atlas.tile(42, frame / 12.0, 100, 100);
        for (int i = 0; i < tile.width(); ++i) {
            assert(qAlpha(tile.pixel(i, 0)) == 0 && qAlpha(tile.pixel(i, 15)) == 0);
            assert(qAlpha(tile.pixel(0, i)) == 0 && qAlpha(tile.pixel(15, i)) == 0);
        }
    }
    assert(qAlpha(steady.pixel(8, 8)) > qAlpha(steady.pixel(2, 2)));
    assert(qBlue(steady.pixel(2, 2)) > qBlue(steady.pixel(8, 8)));
    using namespace terrain_room_light;
    using T = RoomTerrainEnum;
    using L = RoomLightEnum;
    for (const auto terrain : {T::INDOORS, T::TUNNEL, T::CAVERN}) {
        assert(classify(terrain, L::LIT, "") == Evidence::ENCLOSED_LIT);
        assert(classify(terrain, L::DARK, "") == Evidence::NONE);
        assert(classify(terrain, L::UNDEFINED, "") == Evidence::NONE);
    }
    for (const auto terrain : {T::CITY, T::FIELD, T::FOREST, T::ROAD, T::UNDEFINED})
        assert(classify(terrain, L::LIT, "") == Evidence::NONE);
    for (const auto description :
         {"Torches flicker in their hangers near the doorways to the north and west.",
          "The only light comes from a blazing fireplace.",
          "A low table bearing a single flickering candle sits near the centre.",
          "There are lanterns lit at intervals around the garden.",
          "A fireplace on the northern wall holds four large lanterns which light the room.",
          "A torch hangs from a bracket, its flame is burning brightly.",
          "Several glowing\ncoals provide warmth."})
        assert(classify(T::INDOORS, L::UNDEFINED, description) == Evidence::DESCRIPTION);
    for (const auto description :
         {"A stone fireplace against a wall is ready to provide warmth in winter.",
          "A hearth full of cold ashes stands in the corner.",
          "You could find firewood and start a nice fire.",
          "Candles and lanterns, when lit, set this room aglow.",
          "The store has lanterns for sale.",
          "An unlit torch hangs from the northern wall.",
          "No torches light the room.",
          "There is no fire burning here.",
          "The fire is extinguished.",
          "The sun lights a pile of cold embers.",
          "A small kitchen and a stone clad fireplace cover the eastern wall.",
          "You see light beyond the doorway."})
        assert(classify(T::INDOORS, L::UNDEFINED, description) == Evidence::NONE);
    assert(classify(T::CAVERN, L::DARK, "A candle flickers on the table.") == Evidence::DESCRIPTION);
    std::set<std::array<uint8_t, 4>> patterns;
    for (uint32_t id = 0; id < 1000; ++id) {
        const auto a = variation(id);
        assert(a == variation(id));
        assert(a[0] >= 184 && a[3] >= 64);
        patterns.insert(a);
    }
    assert(patterns.size() > 990);
    Observation observation;
    const auto json = [](const char *s) { return QJsonDocument::fromJson(s).object(); };
    observation.room(json(R"({"id":101})"));
    assert(!observation.appliesTo(101));
    observation.vitals(json(R"({"light":"!"})"));
    assert(observation.appliesTo(101));
    assert(!observation.appliesTo(102));
    observation.vitals(json(R"({"hp":100})"));
    assert(observation.appliesTo(101));
    observation.room(json(R"({"id":101})"));
    assert(observation.appliesTo(101));
    for (const auto value : {"o", "*", ")", "?"}) {
        observation.vitals(QJsonObject{{QStringLiteral("light"), QString::fromLatin1(value)}});
        assert(!observation.appliesTo(101));
    }
    observation.vitals(json(R"({"light":"!"})"));
    observation.clear();
    assert(!observation.appliesTo(101));
    observation.room(json(R"({"id":102})"));
    assert(!observation.appliesTo(102));
    observation.clear();
    observation.vitals(json(R"({"light":"!"})")); // Vitals before Room.Info.
    assert(!observation.appliesTo(102));
    observation.room(json(R"({"id":4294967295})"));
    assert(observation.appliesTo(0xffffffffu));
    observation.room(json(R"({"id":103})"));
    assert(!observation.appliesTo(103));
    observation.clear();
    observation.room(json(R"({})")); // Fallback for maps without server IDs.
    observation.vitals(json(R"({"light":"!"})"));
    assert(observation.appliesTo(0));
    observation.vitals(json(R"({"light":null})"));
    assert(!observation.appliesTo(0));
    std::cout << "Room evidence, negation, saved-ID variation and live light lifecycle passed\n";
}
