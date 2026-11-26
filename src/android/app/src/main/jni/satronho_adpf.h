// SPDX-FileCopyrightText: Copyright 2026 Satronho NX
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

namespace Core {
class System;
}

// Satronho NX: Android performance hints (ADPF). While a game runs, the emulator's busy threads
// (the emulated CPU cores, the GPU threads and the Vulkan worker) join a hint session and each
// presented frame reports how long it took. The phone then raises the clocks of those threads
// as soon as the frames run late, instead of waiting for its governor to notice, and lowers them
// again when the game keeps its pace. Phones without the API (or with it off) are left alone.
namespace Satronho::Adpf {

/// Starts the hint session for the running game. Safe to call when the phone lacks the API.
void Start(Core::System& system);

/// Ends the hint session and logs a one-line summary.
void Stop();

/// On by default; the app's switch turns it off for the next game.
void SetEnabled(bool enabled);
bool IsEnabled();

/// The phone offers performance hints (Android 13 or later with a supporting kernel).
bool IsSupported();

} // namespace Satronho::Adpf
