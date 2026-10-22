// SPDX-License-Identifier: GPL-2.0-or-later
#include "display/TerrainLightMask.h"
#include "display/TerrainRoomLightRules.h"

#include <algorithm>
#include <cassert>
#include <iostream>
#include <set>

#include <QColor>
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
            assert(qAlpha(tile.pixel(i, 0)) == 0 && qAlpha(tile.pixel(i, tile.height() - 1)) == 0);
            assert(qAlpha(tile.pixel(0, i)) == 0 && qAlpha(tile.pixel(tile.width() - 1, i)) == 0);
        }
    }
    assert(qAlpha(steady.pixel(16, 16)) > qAlpha(steady.pixel(10, 10)));
    assert(qBlue(steady.pixel(10, 10)) > qBlue(steady.pixel(16, 16)));
    int lowest = 255;
    int highest = 0;
    for (int tick = 0; tick < 60; ++tick) {
        const auto tile = atlas.tile(42, tick * .084, 100, 100);
        const int alpha = qAlpha(tile.pixel(16, 16));
        lowest = std::min(lowest, alpha);
        highest = std::max(highest, alpha);
    }
    assert(highest - lowest >= 40); // Full flicker must be readily distinguishable from steady.
    using namespace terrain_room_light;
    assert(streetLampsOn(MumeTimeEnum::DAWN));
    assert(streetLampsOn(MumeTimeEnum::DUSK));
    assert(streetLampsOn(MumeTimeEnum::NIGHT));
    assert(!streetLampsOn(MumeTimeEnum::DAY));
    assert(!streetLampsOn(MumeTimeEnum::UNKNOWN));
    WallNeighborhood open;
    open.walls.fill(0);
    auto clear = steady;
    MaskAtlas::applyClip(clear, MaskAtlas::clip(open));
    for (int y = 0; y < clear.height(); ++y)
        for (int x = 0; x < clear.width(); ++x) {
            assert(qAlpha(clear.pixel(x, y)) == qAlpha(steady.pixel(x, y)));
            assert(qBlue(clear.pixel(x, y)) == 0); // Open terrain has no invented wall corners.
        }
    // The circular pool extends past the owning square along every axis.
    for (const auto point : {std::array<int, 2>{5, 16}, {26, 16}, {16, 5}, {16, 26}})
        assert(qAlpha(clear.pixel(point[0], point[1])) > 0);
    assert(qAlpha(clear.pixel(27, 16)) >= 10); // The free rim must remain visible.
    assert(qAlpha(clear.pixel(3, 3)) == 0);    // Circular, not another square tile.
    for (unsigned dir = 0; dir < 4; ++dir) {
        auto neighbours = open;
        neighbours.walls[4] = static_cast<uint8_t>(1u << dir);
        auto clipped = steady;
        MaskAtlas::applyClip(clipped, MaskAtlas::clip(neighbours));
        for (int y = 0; y < 32; ++y)
            for (int x = 0; x < 32; ++x) {
                const bool outside = dir == 0   ? y >= 23 && x >= 8 && x < 24
                                     : dir == 1 ? y <= 8 && x >= 8 && x < 24
                                     : dir == 2 ? x >= 23 && y >= 8 && y < 24
                                                : x <= 8 && y >= 8 && y < 24;
                if (outside) {
                    assert(qAlpha(clipped.pixel(x, y)) == 0);
                    assert(qBlue(clipped.pixel(x, y)) == 0);
                }
            }
        // A wall recorded on only the opposite side must block identically.
        auto other = open;
        const std::array<int, 4> neighbour{7, 1, 5, 3};
        other.walls[static_cast<size_t>(neighbour[dir])] = static_cast<uint8_t>(1u << (dir ^ 1u));
        auto fromOther = steady;
        MaskAtlas::applyClip(fromOther, MaskAtlas::clip(other));
        // Compare the blocked half-plane; other walls may guard the neighbour itself.
        for (int y = 0; y < 32; ++y)
            for (int x = 0; x < 32; ++x)
                if (dir == 0   ? y >= 23 && x >= 8 && x < 24
                    : dir == 1 ? y <= 8 && x >= 8 && x < 24
                    : dir == 2 ? x >= 23 && y >= 8 && y < 24
                               : x <= 8 && y >= 8 && y < 24)
                    assert(qAlpha(fromOther.pixel(x, y)) == 0);
    }
    WallNeighborhood closed;
    const auto closedClip = MaskAtlas::clip(closed);
    int cornerLow = 255, cornerHigh = 0, edgeLow = 255, edgeHigh = 0;
    for (int tick = 0; tick < 60; ++tick) {
        auto frame = atlas.tile(42, tick * .084, 100, 100);
        edgeLow = std::min(edgeLow, qAlpha(frame.pixel(26, 16)));
        edgeHigh = std::max(edgeHigh, qAlpha(frame.pixel(26, 16)));
        MaskAtlas::applyClip(frame, closedClip);
        cornerLow = std::min(cornerLow, qBlue(frame.pixel(9, 9)));
        cornerHigh = std::max(cornerHigh, qBlue(frame.pixel(9, 9)));
        for (int y = 0; y < 32; ++y)
            for (int x = 0; x < 32; ++x)
                if (x <= 8 || x >= 23 || y <= 8 || y >= 23)
                    assert(qAlpha(frame.pixel(x, y)) == 0 && qBlue(frame.pixel(x, y)) == 0);
    }
    assert(cornerHigh - cornerLow >= 5 && edgeHigh - edgeLow >= 5);
    QImage composed(48, 32, QImage::Format_RGBA8888);
    composed.fill(0);
    QImage reversed = composed;
    const auto second = atlas.tile(43, .4, 100, 100);
    MaskAtlas::composite(composed, steady, 0, 0);
    MaskAtlas::composite(composed, second, 16, 0);
    MaskAtlas::composite(reversed, second, 16, 0);
    MaskAtlas::composite(reversed, steady, 0, 0);
    assert(composed == reversed); // Overlapping pools must not erase each other or add brightness.
    assert(qAlpha(composed.pixel(20, 16)) >= qAlpha(steady.pixel(20, 16)));
    // A tiny change in which light is brighter must not switch its shadow or
    // temperature. Brightness stays bounded rather than adding overlapping pools.
    const auto overlap = [](const int a, const int b) {
        QImage image(1, 1, QImage::Format_RGBA8888);
        image.fill(0);
        QImage firstPixel(1, 1, QImage::Format_RGBA8888),
            secondPixel(1, 1, QImage::Format_RGBA8888);
        firstPixel.setPixelColor(0, 0, QColor(255, 184, 30, a));
        secondPixel.setPixelColor(0, 0, QColor(255, 184, 0, b));
        MaskAtlas::composite(image, firstPixel, 0, 0);
        MaskAtlas::composite(image, secondPixel, 0, 0);
        return image.pixelColor(0, 0);
    };
    const auto brighterFirst = overlap(101, 100), brighterSecond = overlap(100, 101);
    assert(brighterFirst == brighterSecond);
    assert(brighterFirst.alpha() == 101 && brighterFirst.blue() == 30);
    // An open join between two walled corridor rooms is not another corner.
    auto corridor = open;
    for (int x = 0; x < 3; ++x)
        corridor.walls[static_cast<size_t>(3 + x)] = 3;
    auto corridorTile = steady;
    MaskAtlas::applyClip(corridorTile, MaskAtlas::clip(corridor));
    for (int x = 8; x < 29; ++x) {
        assert(qBlue(corridorTile.pixel(x, 16)) == 0);
        assert(qAlpha(corridorTile.pixel(x, 16)) == qAlpha(steady.pixel(x, 16)));
    }
    assert(qBlue(corridorTile.pixel(22, 9)) > 0); // Real wall shade survives across the join.
    using T = RoomTerrainEnum;
    using L = RoomLightEnum;
    for (const auto terrain : {T::INDOORS, T::TUNNEL, T::CAVERN}) {
        assert(classify(terrain, L::LIT, "") == Evidence::NONE);
        assert(classify(terrain, L::DARK, "") == Evidence::NONE);
        assert(classify(terrain, L::UNDEFINED, "") == Evidence::NONE);
    }
    for (const auto terrain : {T::CITY, T::FIELD, T::FOREST, T::ROAD, T::UNDEFINED}) {
        assert(classify(terrain, L::LIT, "") == Evidence::NONE);
        assert(classify(terrain, L::UNDEFINED, "") == Evidence::NONE);
    }
    for (const auto description :
         {"Torches flicker in their hangers near the doorways to the north and west.",
          "The only light comes from a blazing fireplace.",
          "A low table bearing a single flickering candle sits near the centre.",
          "There are lanterns lit at intervals around the garden.",
          "A fireplace on the northern wall holds four large lanterns which light the room.",
          "A torch hangs from a bracket, its flame is burning brightly.",
          "Several glowing\ncoals provide warmth.",
          "A forge blazes against the cold stone walls.",
          "The forge glows red-hot.",
          "A roaring forge fills the workshop with light.",
          "An unused forge stands here, but a brazier burns near the door.",
          "An unlit torch rests on a bench; glowing coals fill the forge.",
          "The furnace is hot.",
          "A smouldering firepit warms the chamber."}) {
        const auto evidence = classify(T::INDOORS, L::UNDEFINED, description);
        if (evidence != Evidence::DESCRIPTION)
            std::cerr << "Missed source: " << description << '\n';
        assert(evidence == Evidence::DESCRIPTION);
    }
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
          "You see light beyond the doorway.",
          "A cold forge occupies the corner.",
          "The forge is not burning.",
          "An unused forge and an unlit torch stand here.",
          "The forge could be lit.",
          "There is a forge against the wall.",
          "The forge is cold, its embers extinguished."}) {
        const auto evidence = classify(T::INDOORS, L::UNDEFINED, description);
        if (evidence != Evidence::NONE)
            std::cerr << "False source: " << description << '\n';
        assert(evidence == Evidence::NONE);
    }
    assert(classify(T::CAVERN, L::DARK, "A candle flickers on the table.") == Evidence::DESCRIPTION);
    assert(classify(T::FIELD, L::LIT, "Sunlight lights this clearing.") == Evidence::NONE);
    assert(classify(T::INDOORS, L::LIT, "A cold forge stands here.") == Evidence::NONE);
    assert(classify(T::CITY, L::LIT, "A brazier burns here.") == Evidence::DESCRIPTION);
    for (const auto description : {"A street-lamp stands here.",
                                   "The street lamp is unlit.",
                                   "Streetlamps line the avenue.",
                                   "A street-lamp shines brightly."}) {
        assert(classify(T::CITY, L::UNDEFINED, description) == Evidence::STREET_LAMP);
        assert(classify(T::INDOORS, L::UNDEFINED, description) == Evidence::DESCRIPTION);
    }
    assert(classify(T::CITY, L::UNDEFINED, "A forge blazes beside a street-lamp.")
           == Evidence::DESCRIPTION);
    for (const auto contents : {"A street-lamp stands beside the road.",
                                "A street-lamp sheds a warm glow.",
                                "The street-lamp is unlit.",
                                "Two street-lamps stand here.",
                                "A street\nlamp stands here."}) {
        assert(classify(T::CITY, L::UNDEFINED, "A cobbled street.", contents)
               == Evidence::STREET_LAMP);
        assert(classify(T::INDOORS, L::UNDEFINED, "A hallway.", contents) == Evidence::DESCRIPTION);
    }
    assert(classify(T::CITY, L::LIT, "", "A street-lamp is unlit.") == Evidence::STREET_LAMP);
    assert(classify(T::CITY, L::LIT, "A forge blazes.", "A street-lamp stands here.")
           == Evidence::DESCRIPTION);
    // Mutable carried items cannot mark arbitrary rooms as permanently lit.
    assert(classify(T::CITY, L::UNDEFINED, "", "A lit torch and a burning candle lie here.")
           == Evidence::NONE);
    assert(classify(T::FIELD, L::UNDEFINED, "", "A campfire burns here.") == Evidence::NONE);
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
    assert(observation.appliesTo(103)); // No changed light value was sent.
    assert(!observation.appliesTo(0xffffffffu));
    // Replay the supplied log: one initial ! followed by several movements
    // with Room.Info and no repeated light field.
    observation.clear();
    observation.room(json(R"({"id":8861838})"));
    observation.vitals(json(R"({"light":"!"})"));
    for (const uint32_t id : {9976175u, 8861838u, 9976175u, 7266861u, 9384560u}) {
        const auto old = observation.serverRoom;
        observation.moved();
        assert(!observation.appliesTo(old));
        observation.vitals(json(R"({"hp":100})"));
        observation.room(QJsonObject{{QStringLiteral("id"), static_cast<double>(id)}});
        assert(observation.appliesTo(id));
        assert(!observation.appliesTo(old));
    }
    // A changed light value before the next Room.Info must win.
    observation.moved();
    observation.vitals(json(R"({"light":"o"})"));
    observation.room(json(R"({"id":200})"));
    assert(!observation.appliesTo(200));
    // Both orders work, including when Event.Moved is absent.
    observation.moved();
    observation.vitals(json(R"({"light":"!"})"));
    assert(!observation.appliesTo(200));
    observation.room(json(R"({"id":201})"));
    assert(observation.appliesTo(201));
    observation.room(json(R"({"id":202})"));
    assert(observation.appliesTo(202));
    observation.vitals(json(R"({"light":"*"})"));
    assert(!observation.appliesTo(202));
    observation.clear();
    observation.room(json(R"({"id":203})"));
    assert(!observation.appliesTo(203));
    observation.clear();
    observation.room(json(R"({})")); // Fallback for maps without server IDs.
    observation.vitals(json(R"({"light":"!"})"));
    assert(observation.appliesTo(0));
    observation.vitals(json(R"({"light":null})"));
    assert(!observation.appliesTo(0));
    std::cout << "Room evidence, negation, saved-ID variation and live light lifecycle passed\n";
}
