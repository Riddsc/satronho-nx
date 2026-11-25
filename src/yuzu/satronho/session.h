// SPDX-FileCopyrightText: Copyright 2026 Satronho NX
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <optional>

#include <QString>

#include "common/common_types.h"

namespace Satronho::Session {

/// What a game session left: how long it ran, how it ran and what it cost in shaders.
struct Summary {
    QString game;
    u64 program_id{};
    double minutes{};         ///< With the game running, pauses included
    double typical_fps{};     ///< Median of the per-second frame rates
    double slow_fps{};        ///< The slowest 10 % of those seconds
    int stutters{};           ///< Frames over 100 ms
    int shader_stutters{};    ///< Of those, the ones while shaders were being built
    int shaders_built{};      ///< Shader builds during the session
    bool stream_mode{};       ///< Stream mode was on at some point of the session
};

/// Whether the session summary is on (Satronho menu; on by default).
bool Enabled();
void SetEnabled(bool enabled);

/// Whether the frame-time history must stay on while the meter is hidden: the summary counts
/// stutters from it.
bool WantsFrameTimes();

/// A game started.
void Begin(u64 program_id, const QString& game);

/// Once a second while it runs, with that second's frame rate.
void Tick(double fps);

/// The game stopped: writes the summary to the log and returns it, or nothing when no session
/// ran long enough to say anything (under a minute).
std::optional<Summary> End();

/// The summary in a few lines for the guides' bubble (HTML).
QString ShortText(const Summary& summary);

/// The summary in full for its window (HTML).
QString LongText(const Summary& summary);

} // namespace Satronho::Session
