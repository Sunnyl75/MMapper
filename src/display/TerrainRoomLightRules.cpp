// SPDX-License-Identifier: GPL-2.0-or-later
#include "TerrainRoomLightRules.h"

#include <QRegularExpression>
#include <QString>

namespace terrain_room_light {
Evidence classify(const RoomTerrainEnum terrain,
                  const RoomLightEnum light,
                  const std::string_view description)
{
    if (light == RoomLightEnum::LIT
        && (terrain == RoomTerrainEnum::INDOORS || terrain == RoomTerrainEnum::TUNNEL
            || terrain == RoomTerrainEnum::CAVERN))
        return Evidence::ENCLOSED_LIT;

    // Static descriptions only: contents may contain carried items, mobs, or old campfires.
    static const QRegularExpression split{QStringLiteral("[.!?;\\n]+")};
    static const QRegularExpression source{
        QStringLiteral(
            "\\b(torch(?:es)?|lanterns?|lamps?|candles?|chandeliers?|braziers?|hearths?|"
            "fireplaces?|campfires?|bonfires?|fires?|coals?|embers?|furnaces?|sconces?|candelabr\\w*)\\b"),
        QRegularExpression::CaseInsensitiveOption};
    static const QRegularExpression active{
        QStringLiteral(
            "\\b(blaz(?:e|es|ing)|burn(?:s|ing)|flicker(?:s|ing)?|glow(?:s|ing)?|lit|"
            "light(?:s|ing)|light\\s+(?:the|this)\\s|(?:give|gives|cast|casts|shed|sheds|provide|provides)\\s+(?:a\\s+)?(?:dim\\s+|warm\\s+)?light|illuminat(?:e|es|ing)|radiat(?:e|es|ing))\\b"),
        QRegularExpression::CaseInsensitiveOption};
    static const QRegularExpression
        inactive{QStringLiteral(
                     "\\b(unlit|unlighted|extinguished|cold|ashes|charred|abandoned|unoccupied|"
                     "unused|empty|stocked|merchandise|sell|sells|selling|sold|could|might|would|"
                     "when|if|ready|start|without|no|not)\\b|for sale|burnt.out|burned.out"),
                 QRegularExpression::CaseInsensitiveOption};
    const auto text = QString::fromUtf8(description.data(),
                                        static_cast<qsizetype>(description.size()));
    // Normalize wrapping before sentence splitting: a source and its verb may span lines.
    QString normalized = text;
    normalized.replace(QLatin1Char('\n'), QLatin1Char(' '));
    for (const auto &clause : normalized.split(split, Qt::SkipEmptyParts)) {
        if (inactive.match(clause).hasMatch())
            continue;
        const auto fixture = source.match(clause);
        if (!fixture.hasMatch())
            continue;
        auto matches = active.globalMatch(clause);
        while (matches.hasNext()) {
            const auto verb = matches.next();
            // Bound proximity rather than connecting unrelated ends of a long sentence.
            if (qAbs(verb.capturedStart() - fixture.capturedStart()) <= 90)
                return Evidence::DESCRIPTION;
        }
    }
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
void Observation::room(const QJsonObject &object)
{
    const double number = object.value(QStringLiteral("id")).toDouble();
    const auto id = number > 0.0 && number <= 4294967295.0 ? static_cast<uint32_t>(number) : 0u;
    if (!roomKnown || serverRoom != id) {
        artificial = pending.value_or(false);
        pending.reset();
    }
    serverRoom = id;
    roomKnown = true;
}
void Observation::vitals(const QJsonObject &object)
{
    const auto it = object.find(QStringLiteral("light"));
    if (it == object.end())
        return; // Partial GMCP update: retain the current observation.
    const bool value = it->isString() && it->toString() == QStringLiteral("!");
    if (roomKnown)
        artificial = value;
    else
        pending = value;
}
bool Observation::appliesTo(const uint32_t currentServerRoom) const
{
    // Do not paint the last room while path matching is catching up with Room.Info.
    return roomKnown && artificial && (serverRoom == 0 || serverRoom == currentServerRoom);
}
} // namespace terrain_room_light
