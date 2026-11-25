// SPDX-FileCopyrightText: Copyright 2026 Satronho NX
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <optional>

#include <QDateTime>
#include <QString>

#include "common/common_types.h"

/// «Preparar para directo»: a game's shaders are built before a stream, so they don't take the
/// CPU from the game and the stream while playing. It precompiles all of the game's shaders on
/// the loading screen of its next start and remembers, per game, when that finished.
namespace Satronho::Directo {

/// A game whose preparation finished.
struct Prepared {
    QDateTime when;
    QString build_tag;   ///< The build that prepared it ("build-56")
    u64 built{};         ///< Compute shaders the precache built
};

/// The game's last finished preparation, if any.
std::optional<Prepared> LastPrepared(u64 program_id);

/// Whether the preparation was asked for and the game has not loaded it yet.
bool Pending(u64 program_id);
void SetPending(u64 program_id, bool pending);

/// The loading screen's precache progress for the game loading now.
void NoteWarmProgress(u64 done, u64 total);
/// A game starts loading: forgets the last game's progress.
void ResetWarmProgress();

/// The game finished loading. For a pending one, records the preparation when its precache
/// ran to the end (or had nothing left to build) and returns true; a skipped one stays pending.
/// `left` gets the shaders a skipped precache left.
bool FinishLoad(u64 program_id, u64& left);

/// The text of the game list's menu entry for the game.
QString MenuText(u64 program_id);

/// Automatic stream mode: while OBS, Streamlabs or TikTok LIVE Studio is open, shader builds the
/// game isn't waiting for use half the workers, leaving CPU for the stream's encoder. The shaders a
/// frame waits on are never held back. Checked every few seconds; on by default (Satronho menu).
bool StreamAutoEnabled();
void SetStreamAutoEnabled(bool enabled);

/// The streaming app open now (its executable's name), or an empty string.
QString FindStreamApp();

/// Applies or lifts stream mode after checking for a streaming app. Returns true when it changed.
bool UpdateStreamMode();

/// Whether stream mode is applied now, and for which app.
bool StreamModeOn();
QString StreamModeApp();

} // namespace Satronho::Directo
