#pragma once
// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 The MMapper Authors
#include "../clock/mumemoment.h"

#include <functional>
#include <optional>

class QDockWidget;
class QWidget;

QDockWidget *createTerrainPreviewClock(QWidget *parent,
                                       std::function<void(std::optional<MumeMoment>)> onChanged);
