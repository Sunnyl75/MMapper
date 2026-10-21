// SPDX-License-Identifier: GPL-2.0-or-later
#include "display/TerrainLightingDiagnostics.h"

#include <cassert>
#include <iostream>

#include <QDebug>
#include <QStringList>
#include <QtGlobal>

namespace {
QStringList &messages()
{
    static QStringList lines;
    return lines;
}
void capture(QtMsgType, const QMessageLogContext &, const QString &message)
{
    messages().push_back(message);
}
using Diagnostics = TerrainLightingDiagnostics;
using Cause = Diagnostics::Cause;
void frame(Diagnostics &diagnostics)
{
    diagnostics.paintAttempt(true);
    auto scope = diagnostics.measure(Diagnostics::Stage::FRAME);
}
} // namespace

int main()
{
    const auto previousHandler = qInstallMessageHandler(capture);
    qputenv("MMAPPER_TERRAIN_TIMINGS", "0");
    {
        Diagnostics disabled;
        disabled.settings(true, 100, 35, false, 60);
        disabled.movement();
        disabled.redrawRequested(Cause::MOVEMENT);
        disabled.redrawRequested(Cause::FLICKER);
        disabled.paintAttempt(false);
        frame(disabled);
        disabled.settings(false, 100, 0, false, 60);
        assert(messages().isEmpty());
    }
    qputenv("MMAPPER_TERRAIN_TIMINGS", "1");
    {
        Diagnostics diagnostics;
        diagnostics.settings(true, 100, 35, false, 60);
        diagnostics.movement();
        diagnostics.redrawRequested(Cause::OBSERVATION);
        frame(diagnostics); // An early paint before room matching must retain the trace.
        assert(!messages().join('\n').contains("[terrain-move]"));
        diagnostics.redrawRequested(Cause::MOVEMENT);
        diagnostics.paintAttempt(false);
        diagnostics.redrawRequested(Cause::FLICKER);
        diagnostics.redrawRequested(Cause::MOVEMENT); // Retain the FIRST request timestamp.
        frame(diagnostics);
        const auto log = messages().join('\n');
        assert(log.count("[terrain-move]") == 1);
        assert(log.contains("seq=1 gmcp-to-request="));
        assert(log.contains("movement-requests=2 rejected-paints=1"));
        assert(log.contains("flicker-age-at-request=-1.000ms flicker-ticks-after-request=1"));
        assert(log.contains(
            "frame-requests[movement=1 flicker=1 observation=0 settings=0] max-fps=60"));
        frame(diagnostics); // Must not report the same movement twice.
        assert(messages().join('\n').count("[terrain-move]") == 1);
        messages().clear();
        diagnostics.settings(true, 100, 0, false, 30);
        assert(messages().join('\n').contains("max-fps=30"));
        diagnostics.movement();
        diagnostics.movement(); // Latest event wins when events coalesce before matching.
        diagnostics.redrawRequested(Cause::MOVEMENT);
        frame(diagnostics);
        const auto coalesced = messages().join('\n');
        assert(coalesced.count("[terrain-move]") == 1);
        assert(coalesced.contains("seq=3 gmcp-to-request="));
        assert(coalesced.contains("movement-requests=1 rejected-paints=0"));
        assert(coalesced.contains("flicker-ticks-after-request=0"));
        assert(coalesced.contains(
            "frame-requests[movement=1 flicker=0 observation=0 settings=0] max-fps=30"));
        messages().clear();
        diagnostics.settings(false, 100, 0, false, 30);
        diagnostics.movement();
        diagnostics.redrawRequested(Cause::MOVEMENT);
        frame(diagnostics); // Tracing remains active with illumination disabled.
        assert(messages().join('\n').contains("[terrain-move] seq=4"));
        diagnostics.settings(false, 100, 0, false, 60); // Flush aggregates.
        const auto summary = messages().join('\n');
        assert(summary.contains("movement-request-to-first-paint-attempt"));
        assert(summary.contains("movement-request-to-frame-start"));
        assert(summary.contains("matched-gmcp-to-frame-submit"));
        assert(summary.contains("redraw-requests movement=1 flicker=0"));
    }
    qInstallMessageHandler(previousHandler);
    std::cout << "Movement timing diagnostics checks passed\n";
}
