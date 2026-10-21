// SPDX-License-Identifier: GPL-2.0-or-later
#include "TerrainRoomLightRules.h"

#include <QRegularExpression>
#include <QString>

namespace terrain_room_light {
Evidence classify(const RoomTerrainEnum terrain,
                  const RoomLightEnum /*light*/,
                  const std::string_view description,
                  const std::string_view contents)
{
    // LIT may describe sunlight. It is not evidence of an artificial source,
    // even in an enclosed sector; retain the argument for the existing API.
    // Descriptions are static evidence; contents may contain carried items or mobs.
    const auto insensitive = QRegularExpression::CaseInsensitiveOption;
    static const QString fixturePattern = QStringLiteral(
        "torch(?:es)?|lanterns?|lamps?|candles?|chandeliers?|braziers?|hearths?|"
        "fireplaces?|campfires?|bonfires?|fires?|coals?|embers?|furnaces?|forges?|"
        "smelters?|firepits?|sconces?|candelabr\\w*");
    static const QRegularExpression split{QStringLiteral("[.!?;]+|\\b(?:but|whereas|although)\\b"),
                                          insensitive};
    static const QRegularExpression source{QStringLiteral("\\b(?:%1)\\b").arg(fixturePattern),
                                           insensitive};
    static const QRegularExpression active{
        QStringLiteral(
            "\\b(blaz(?:e|es|ing)|burn(?:s|ing)|flicker(?:s|ing)?|glow(?:s|ing)?|lit|"
            "light(?:s|ing)|light\\s+(?:the|this)\\s|illuminat(?:e|es|ing)|radiat(?:e|es|ing)|roar(?:s|ing)?|"
            "smould(?:er|ers|ering)|smold(?:er|ers|ering)|ablaze|alight|aglow|red.hot|hot)\\b"),
        insensitive};
    static const QRegularExpression
        hypothetical{QStringLiteral(
                         "\\b(could|might|would|when|if|ready|stocked|merchandise|sell|sells|"
                         "selling|sold)\\b|for sale"),
                     insensitive};
    // Negatives must describe the fixture, not unrelated cold stone or an empty
    // chair elsewhere in the sentence. Check each fixture, not just the first.
    static const QRegularExpression
        beforeSource{QStringLiteral(
                         "\\b(?:unlit|unlighted|extinguished|cold|charred|abandoned|unused|"
                         "empty|no|without)\\b(?:\\s+[\\w'-]+){0,3}\\s+$"),
                     insensitive};
    static const QRegularExpression
        afterSource{QStringLiteral("^\\s*(?:(?:is|are|was|were|lies|lie|stands|stand|has|have|gone|"
                                   "been|long|now|all|completely|quite|totally)\\s+){0,4}"
                                   "(?:unlit|unlighted|extinguished|cold|unused|empty|out|not)\\b"),
                    insensitive};
    QString normalized = QString::fromUtf8(description.data(),
                                           static_cast<qsizetype>(description.size()));
    normalized.replace(QLatin1Char('\n'), QLatin1Char(' '));
    static const QRegularExpression streetLamp{QStringLiteral("\\bstreet(?:[-\\s]+)?lamps?\\b"),
                                               insensitive};
    // Street lamps are often object lines in saved contents, not room prose.
    // Only this fixed-fixture hint is read there: carried torches/candles and
    // transient campfires must not become permanent room illumination.
    const QString savedContents = QString::fromUtf8(contents.data(),
                                                    static_cast<qsizetype>(contents.size()));
    const bool hasStreetLamp = streetLamp.match(normalized).hasMatch()
                               || streetLamp.match(savedContents).hasMatch();
    if (hasStreetLamp
        && (terrain == RoomTerrainEnum::INDOORS || terrain == RoomTerrainEnum::TUNNEL
            || terrain == RoomTerrainEnum::CAVERN))
        return Evidence::DESCRIPTION;
    // Its current on/off wording is deliberately ignored. Other permanent
    // sources in the same room can still establish all-day illumination.
    normalized.replace(streetLamp, QStringLiteral(" "));

    for (const auto &clause : normalized.split(split, Qt::SkipEmptyParts)) {
        if (hypothetical.match(clause).hasMatch())
            continue;
        auto fixtures = source.globalMatch(clause);
        while (fixtures.hasNext()) {
            const auto fixture = fixtures.next();
            const auto prefix = clause.left(fixture.capturedStart());
            const auto suffix = clause.mid(fixture.capturedEnd());
            if (beforeSource.match(prefix).hasMatch() || afterSource.match(suffix).hasMatch())
                continue;
            auto matches = active.globalMatch(clause);
            while (matches.hasNext()) {
                const auto verb = matches.next();
                if (qAbs(verb.capturedStart() - fixture.capturedStart()) <= 90)
                    return Evidence::DESCRIPTION;
            }
        }
    }
    if (hasStreetLamp)
        return Evidence::STREET_LAMP; // Retain the outdoor day/night policy.
    return Evidence::NONE;
}

std::array<uint8_t, 4> variation(uint32_t savedRoomId)
{
    savedRoomId ^= savedRoomId >> 16;
    savedRoomId *= 0x7feb352du;
    savedRoomId ^= savedRoomId >> 15;
    savedRoomId *= 0x846ca68bu;
    savedRoomId ^= savedRoomId >> 16;
    return {static_cast<uint8_t>(184u + savedRoomId % 64u),
            static_cast<uint8_t>((savedRoomId >> 8) & 255u),
            static_cast<uint8_t>((savedRoomId >> 16) & 255u),
            static_cast<uint8_t>(64u + (savedRoomId >> 24) % 192u)};
}

void Observation::clear()
{
    *this = Observation{};
}
void Observation::moved()
{
    // Char.Vitals is incremental. Keep the latest light value, but do not
    // illuminate a room until fresh Room.Info identifies the destination.
    serverRoom = 0;
    roomKnown = false;
}
void Observation::room(const QJsonObject &object)
{
    const double number = object.value(QStringLiteral("id")).toDouble();
    serverRoom = number > 0.0 && number <= 4294967295.0 ? static_cast<uint32_t>(number) : 0u;
    roomKnown = true;
}
void Observation::vitals(const QJsonObject &object)
{
    const auto it = object.find(QStringLiteral("light"));
    if (it == object.end())
        return; // Partial update: the latest light state is unchanged, even across moves.
    artificial = it->isString() && it->toString() == QStringLiteral("!");
}
bool Observation::appliesTo(const uint32_t currentServerRoom) const
{
    // Do not paint the last room while path matching is catching up with Room.Info.
    return roomKnown && artificial && (serverRoom == 0 || serverRoom == currentServerRoom);
}
} // namespace terrain_room_light
