// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <array>
#include <chrono>
#include <source_location>

#include "common/common_types.h"
#include "video_core/vulkan_common/vulkan_wrapper.h"

namespace Vulkan::SlowWait {

// Satronho NX: finds the GPU work behind long waits for the GPU. Every pipeline change is noted
// with the command buffer tick it was recorded in, and every compute dispatch is timestamped on
// the GPU; a wait for the GPU longer than EDEN_NX2_SLOW_WAIT_MS (default 250, 0 turns it off)
// logs the dispatches of the ticks it waited for by GPU time, then their pipelines, most used
// first, so a shader that runs for seconds can be named by its hash.

/// A dispatch without a timestamp slot.
inline constexpr u32 NoSlot = ~0U;

/// Whether the detector is on (read once).
[[nodiscard]] bool Enabled();

/// Scheduler: the timestamp pool for dispatch times, `slots` pairs of queries (null pool: none).
void SetTimestampPool(const vk::Device* device, VkQueryPool pool, u32 slots, u32 valid_bits,
                      float period_ns);

/// The timestamp pool set by the scheduler.
[[nodiscard]] VkQueryPool TimestampPool();

/// A draw with this graphics pipeline was recorded at `tick`.
void NoteGraphics(u64 tick, const std::array<u64, 6>& stage_hashes);

/// A dispatch of this compute shader is being recorded at `tick`, with its grid size. Returns its
/// timestamp slot for ResetSlot and TimeDispatch, or NoSlot.
[[nodiscard]] u32 NoteCompute(u64 tick, u64 shader_hash, u32 grid_x, u32 grid_y, u32 grid_z);

/// Worker thread, first thing in the recorded command (outside a render pass): clears the slot,
/// so a dispatch that ends up skipped reports no time instead of an old one.
inline void ResetSlot(vk::CommandBuffer cmdbuf, u32 slot) {
    if (slot != NoSlot) {
        cmdbuf.ResetQueryPool(TimestampPool(), slot * 2, 2);
    }
}

/// Worker thread: timestamps the dispatch that `work` records.
template <typename Func>
void TimeDispatch(vk::CommandBuffer cmdbuf, u32 slot, Func&& work) {
    if (slot == NoSlot) {
        work();
        return;
    }
    const VkQueryPool pool = TimestampPool();
    cmdbuf.WriteTimestamp(VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, pool, slot * 2);
    work();
    cmdbuf.WriteTimestamp(VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, pool, slot * 2 + 1);
}

/// The thread about to wait sets where the wait comes from; cleared after.
void SetWaitSite(const std::source_location* site);

/// Where the current thread's wait comes from, if set.
[[nodiscard]] const std::source_location* WaitSite();

/// Called after a wait for `tick` that started with `known_done` completed on the GPU.
void ReportWait(std::chrono::steady_clock::duration waited, u64 tick, u64 known_done);

} // namespace Vulkan::SlowWait
