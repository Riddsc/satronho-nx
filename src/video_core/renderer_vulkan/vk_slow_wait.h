// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <array>
#include <chrono>
#include <source_location>

#include "common/common_types.h"

namespace Vulkan::SlowWait {

// Satronho NX: finds the GPU work behind long waits for the GPU. Every pipeline change is noted
// with the command buffer tick it was recorded in; a wait for the GPU longer than
// EDEN_NX2_SLOW_WAIT_MS (default 250, 0 turns it off) logs the pipelines of the ticks it waited
// for, most used first, so a shader that runs for seconds can be named by its hash.

/// Whether the detector is on (read once).
[[nodiscard]] bool Enabled();

/// A draw with this graphics pipeline was recorded at `tick`.
void NoteGraphics(u64 tick, const std::array<u64, 6>& stage_hashes);

/// A dispatch of this compute shader was recorded at `tick`, with its grid size.
void NoteCompute(u64 tick, u64 shader_hash, u32 grid_x, u32 grid_y, u32 grid_z);

/// The thread about to wait sets where the wait comes from (Scheduler::Finish); cleared after.
void SetWaitSite(const std::source_location* site);

/// Called after a wait for `tick` that started with `known_done` completed on the GPU.
void ReportWait(std::chrono::steady_clock::duration waited, u64 tick, u64 known_done);

} // namespace Vulkan::SlowWait
