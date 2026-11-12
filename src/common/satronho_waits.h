// SPDX-FileCopyrightText: Copyright 2026 Satronho NX
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <array>
#include <chrono>
#include <string>
#include <string_view>
#include <vector>

#include "common/common_types.h"

/// Satronho NX: what the game's threads wait on. The cores row showed the guest cores working less
/// when the frame rate drops (build 41): they wait. Each guest wait shorter than ShortWaitNs (a
/// hand-off inside a frame, not an idle worker) is added to a kind and to a label (the event,
/// service or object it waited on). Totals only grow; readers keep the previous snapshot and take
/// the difference (the stutter trace's 10 s summary and the meter).
namespace Satronho::Waits {

enum class Kind : u8 {
    GpuRead,     ///< A guest read of GPU-written memory waited for the GPU (GPU::OnCPURead)
    GpuSync,     ///< WaitSynchronization on a GPU syncpoint event (nvhost-ctrl)
    Present,     ///< WaitSynchronization on presentation (buffer queue, vsync, display)
    Threads,     ///< The game's own threads: condition variables, its events, thread exits
    Sleep,       ///< SleepThread
    Services,    ///< IPC to an emulated service, waiting for its reply
    Polling,     ///< Idle or periodic waits that hold no frame back: address-arbiter waits (idle
                 ///< workers checking for work), timed-out waits and the audio thread's wake-ups
    Count,
};

inline constexpr std::array<std::string_view, static_cast<size_t>(Kind::Count)> kind_names{
    "gpu_read", "gpu_sync", "present", "threads", "sleep", "services", "polling",
};

/// Waits this long or longer are idle threads, not hand-offs: left out.
inline constexpr u64 ShortWaitNs = 100'000'000;

[[nodiscard]] inline u64 NowNs() {
    return static_cast<u64>(std::chrono::duration_cast<std::chrono::nanoseconds>(
                                std::chrono::steady_clock::now().time_since_epoch())
                                .count());
}

/// Adds a wait. `label` may be empty (the kind is enough). Waits of ShortWaitNs or more are
/// dropped unless `always` (a GPU read is never an idle thread).
void Add(Kind kind, std::string_view label, u64 ns, bool always = false);

/// Adds to a label only (a detail of a wait already counted with Add).
void AddLabel(Kind kind, std::string_view label, u64 ns);

/// Remembers a service port's name for the IPC labels.
void NamePort(const void* port, std::string name);
[[nodiscard]] std::string PortName(const void* port);

struct Totals {
    std::array<u64, static_cast<size_t>(Kind::Count)> ns{};
    std::array<u64, static_cast<size_t>(Kind::Count)> count{};
};

struct Labeled {
    std::string label; ///< "kind:label"
    u64 ns{};
    u64 count{};
};

[[nodiscard]] Totals ReadTotals();
[[nodiscard]] std::vector<Labeled> ReadLabels();

} // namespace Satronho::Waits
