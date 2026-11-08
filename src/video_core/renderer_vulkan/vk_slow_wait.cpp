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

struct Slot {
    u64 tick{};
    u64 hash{};
    u32 grid_x{}, grid_y{}, grid_z{};
};

// Timestamp slots, reused in turn: ~2 s of dispatches at Ocarina's ~4,500 a second.
const vk::Device* timestamp_device{};
VkQueryPool timestamp_pool{};
std::vector<Slot> slots;
u32 slot_next{};
u64 timestamp_mask{};
double timestamp_period_ns{};

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

// Caller holds ring_mutex. The GPU time of the timestamped dispatches in the ticks the GPU had
// not finished, by shader, slowest first.
std::string DispatchTimes(u64 tick, u64 known_done) {
    if (slots.empty() || timestamp_device == nullptr) {
        return {};
    }
    struct Time {
        const Slot* slot{};
        double total_ms{};
        double max_ms{};
        u32 count{};
    };
    std::vector<Time> times;
    std::unordered_map<u64, size_t> index;
    double all_ms = 0.0;
    u32 timed = 0;
    for (u32 i = 0; i < slots.size(); ++i) {
        const Slot& slot = slots[i];
        if (slot.tick <= known_done || slot.tick > tick) {
            continue;
        }
        // Each query: the 64-bit value, then its availability.
        std::array<u64, 4> data{};
        const VkResult result = timestamp_device->GetQueryResults(
            timestamp_pool, i * 2, 2, sizeof(data), data.data(), sizeof(u64) * 2,
            VK_QUERY_RESULT_64_BIT | VK_QUERY_RESULT_WITH_AVAILABILITY_BIT);
        if ((result != VK_SUCCESS && result != VK_NOT_READY) || data[1] == 0 || data[3] == 0) {
            continue;
        }
        const u64 ticks = (data[2] - data[0]) & timestamp_mask;
        const double ms = static_cast<double>(ticks) * timestamp_period_ns / 1e6;
        all_ms += ms;
        ++timed;
        const auto [it, inserted] = index.try_emplace(slot.hash, times.size());
        if (inserted) {
            times.push_back({&slot});
        }
        Time& time = times[it->second];
        time.total_ms += ms;
        time.max_ms = std::max(time.max_ms, ms);
        ++time.count;
    }
    if (timed == 0) {
        return "no timed dispatches";
    }
    std::ranges::sort(times, [](const Time& a, const Time& b) { return a.total_ms > b.total_ms; });
    std::string text = fmt::format("{} dispatches took {:.1f} ms on the GPU, slowest first:",
                                   timed, all_ms);
    for (size_t i = 0; i < times.size() && i < 12; ++i) {
        const Time& time = times[i];
        text += fmt::format("\n    compute {:016x} {:.1f} ms (x{}, longest {:.1f} ms) grid {}x{}x{}",
                            time.slot->hash, time.total_ms, time.count, time.max_ms,
                            time.slot->grid_x, time.slot->grid_y, time.slot->grid_z);
    }
    return text;
}

} // Anonymous namespace

bool Enabled() {
    return ThresholdMs() != 0;
}

void SetTimestampPool(const vk::Device* device, VkQueryPool pool, u32 slot_count, u32 valid_bits,
                      float period_ns) {
    std::scoped_lock lock{ring_mutex};
    timestamp_device = device;
    timestamp_pool = pool;
    slots.assign(pool != VK_NULL_HANDLE ? slot_count : 0, Slot{});
    slot_next = 0;
    timestamp_mask = valid_bits >= 64 ? ~0ULL : (1ULL << valid_bits) - 1;
    timestamp_period_ns = period_ns;
    // A new session starts its ticks over: forget the previous one's waits too.
    ring_used = 0;
    ring_next = 0;
}

VkQueryPool TimestampPool() {
    return timestamp_pool;
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

u32 NoteCompute(u64 tick, u64 shader_hash, u32 grid_x, u32 grid_y, u32 grid_z) {
    if (!Enabled()) {
        return NoSlot;
    }
    std::scoped_lock lock{ring_mutex};
    u32 slot = NoSlot;
    if (!slots.empty()) {
        slot = slot_next;
        slot_next = (slot_next + 1) % static_cast<u32>(slots.size());
        slots[slot] = {.tick = tick, .hash = shader_hash, .grid_x = grid_x, .grid_y = grid_y,
                       .grid_z = grid_z};
    }
    if (ring_used != 0) {
        Entry& last = Last();
        if (last.compute && last.tick == tick && last.hash == shader_hash &&
            last.grid_x == grid_x && last.grid_y == grid_y && last.grid_z == grid_z) {
            ++last.count;
            return slot;
        }
    }
    Push({.tick = tick,
          .hash = shader_hash,
          .count = 1,
          .grid_x = grid_x,
          .grid_y = grid_y,
          .grid_z = grid_z,
          .compute = true});
    return slot;
}

void SetWaitSite(const std::source_location* site) {
    wait_site = site;
}

const std::source_location* WaitSite() {
    return wait_site;
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
        const std::string timed = DispatchTimes(tick, known_done);
        std::string list;
        for (size_t i = 0; i < work.size() && i < 64; ++i) {
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
        if (!timed.empty()) {
            LOG_WARNING(Render_Vulkan, "Slow GPU wait for tick {}: {}", tick, timed);
        }
    }
}

} // namespace Vulkan::SlowWait
