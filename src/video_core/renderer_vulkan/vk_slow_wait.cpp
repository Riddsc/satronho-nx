// SPDX-License-Identifier: GPL-3.0-or-later

#include <algorithm>
#include <atomic>
#include <cstdlib>
#include <mutex>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include <fmt/format.h>

#include "common/logging.h"
#include "video_core/renderer_vulkan/vk_slow_wait.h"

namespace Vulkan::SlowWait {
namespace {

struct Entry {
    u64 tick{};
    u64 hash{};                  ///< Hash of the graphics stage hashes, or the compute shader hash
    std::array<u64, 6> stages{}; ///< Graphics stage hashes (zero for compute)
    u32 count{};                 ///< Draws or dispatches in a row with this pipeline
    u32 grid_x{}, grid_y{}, grid_z{};
    bool compute{};
};

// About a second of pipeline changes at Ocarina's rate; older entries are long finished.
constexpr size_t RingSize = 4096;

std::mutex ring_mutex;
std::array<Entry, RingSize> ring;
size_t ring_next{};
size_t ring_used{};
std::atomic<u32> reports{};
thread_local const std::source_location* wait_site{};

u32 ThresholdMs() {
    static const u32 value = [] {
        const char* env = std::getenv("EDEN_NX2_SLOW_WAIT_MS");
        return env != nullptr ? static_cast<u32>(std::strtoul(env, nullptr, 10)) : 250U;
    }();
    return value;
}

Entry& Last() {
    return ring[(ring_next + RingSize - 1) % RingSize];
}

void Push(const Entry& entry) {
    ring[ring_next] = entry;
    ring_next = (ring_next + 1) % RingSize;
    ring_used = std::min(ring_used + 1, RingSize);
}

std::string_view FileName(const char* path) {
    std::string_view name{path};
    if (const auto slash = name.find_last_of("/\\"); slash != std::string_view::npos) {
        name.remove_prefix(slash + 1);
    }
    return name;
}

} // Anonymous namespace

bool Enabled() {
    return ThresholdMs() != 0;
}

void NoteGraphics(u64 tick, const std::array<u64, 6>& stage_hashes) {
    if (!Enabled()) {
        return;
    }
    u64 pipeline_hash = 0xcbf29ce484222325ULL;
    for (const u64 stage : stage_hashes) {
        pipeline_hash = (pipeline_hash ^ stage) * 0x100000001b3ULL;
    }
    std::scoped_lock lock{ring_mutex};
    if (ring_used != 0) {
        Entry& last = Last();
        if (!last.compute && last.tick == tick && last.hash == pipeline_hash) {
            ++last.count;
            return;
        }
    }
    Push({.tick = tick, .hash = pipeline_hash, .stages = stage_hashes, .count = 1});
}

void NoteCompute(u64 tick, u64 shader_hash, u32 grid_x, u32 grid_y, u32 grid_z) {
    if (!Enabled()) {
        return;
    }
    std::scoped_lock lock{ring_mutex};
    if (ring_used != 0) {
        Entry& last = Last();
        if (last.compute && last.tick == tick && last.hash == shader_hash &&
            last.grid_x == grid_x && last.grid_y == grid_y && last.grid_z == grid_z) {
            ++last.count;
            return;
        }
    }
    Push({.tick = tick,
          .hash = shader_hash,
          .count = 1,
          .grid_x = grid_x,
          .grid_y = grid_y,
          .grid_z = grid_z,
          .compute = true});
}

void SetWaitSite(const std::source_location* site) {
    wait_site = site;
}

void ReportWait(std::chrono::steady_clock::duration waited, u64 tick, u64 known_done) {
    if (!Enabled()) {
        return;
    }
    const auto ms = std::chrono::duration<double, std::milli>(waited).count();
    if (ms < static_cast<double>(ThresholdMs())) {
        return;
    }
    // A session with a broken shader could wait hundreds of times: the first 100 are enough.
    if (reports.fetch_add(1, std::memory_order_relaxed) >= 100) {
        return;
    }
    struct Work {
        const Entry* first{};
        u64 count{};
    };
    std::vector<Work> work;
    u64 oldest_tick = tick;
    {
        std::scoped_lock lock{ring_mutex};
        std::unordered_map<u64, size_t> index;
        for (size_t i = 0; i < ring_used; ++i) {
            const Entry& entry = ring[(ring_next + RingSize - 1 - i) % RingSize];
            // The ticks the GPU had not finished when the wait began, up to the one waited for.
            if (entry.tick <= known_done || entry.tick > tick) {
                continue;
            }
            oldest_tick = std::min(oldest_tick, entry.tick);
            const u64 key = entry.hash ^ (entry.compute ? 0x9e3779b97f4a7c15ULL : 0);
            const auto [it, inserted] = index.try_emplace(key, work.size());
            if (inserted) {
                work.push_back({&entry, 0});
            }
            work[it->second].count += entry.count;
        }
        std::ranges::sort(work, [](const Work& a, const Work& b) { return a.count > b.count; });
        std::string list;
        for (size_t i = 0; i < work.size() && i < 24; ++i) {
            const Entry& entry = *work[i].first;
            if (entry.compute) {
                list += fmt::format("\n    compute {:016x} x{} grid {}x{}x{}", entry.hash,
                                    work[i].count, entry.grid_x, entry.grid_y, entry.grid_z);
            } else {
                list += fmt::format("\n    graphics x{} vs {:016x} fs {:016x}", work[i].count,
                                    entry.stages[1] != 0 ? entry.stages[1] : entry.stages[0],
                                    entry.stages[4]);
            }
        }
        const std::string site =
            wait_site != nullptr
                ? fmt::format("{}:{}", FileName(wait_site->file_name()), wait_site->line())
                : std::string{"(unknown)"};
        LOG_WARNING(Render_Vulkan,
                    "Slow GPU wait: {:.0f} ms for tick {} from {} (GPU had finished tick {}); "
                    "{} pipelines in ticks {}-{}, most used first:{}",
                    ms, tick, site, known_done, work.size(), oldest_tick, tick,
                    list.empty() ? std::string{" (none noted)"} : list);
    }
}

} // namespace Vulkan::SlowWait
