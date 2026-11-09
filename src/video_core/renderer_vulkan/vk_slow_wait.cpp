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
    u64 hash{};                  ///< Compute shader hash, or hash of the graphics stage hashes
    std::array<u64, 6> stages{}; ///< Graphics stage hashes (zero for compute)
    u32 grid_x{}, grid_y{}, grid_z{};
    bool compute{true};
};

// Timestamp slots, reused in turn: ~2 s of dispatches at Ocarina's ~4,500 a second (the scheduler
// makes more when draws are timed too).
const vk::Device* timestamp_device{};
bool timestamp_host_reset{};
VkQueryPool timestamp_pool{};
std::vector<Slot> slots;
u32 slot_next{};
u64 timestamp_mask{};
double timestamp_period_ns{};

// Whole submissions: when each reached the queue and its GPU timestamps, so a wait can tell a late
// submission, a GPU that sat idle before starting it (a semaphore), its uploads and its commands
// apart. Ocarina submits ~700 times a second; 2048 entries keep ~3 s.
struct Submit {
    u64 tick{};
    std::chrono::steady_clock::time_point submitted{};
    bool valid{};
};
VkQueryPool submit_pool{};
std::vector<Submit> submits;
u32 submit_next{};
std::mutex submit_mutex;

// What kept the Vulkan worker thread busy: long command chunks and the compute pipeline builds it
// waited for inside them. A wait for a tick the worker submitted late overlaps one of these.
struct WorkerBlock {
    std::chrono::steady_clock::time_point start{};
    std::chrono::steady_clock::time_point end{};
    u64 shader_hash{}; ///< Compute shader whose build it waited for; zero for a whole chunk
};
constexpr size_t WorkerBlockCount = 64;
std::array<WorkerBlock, WorkerBlockCount> worker_blocks{};
size_t worker_block_next{};
std::mutex worker_mutex;
constexpr auto WorkerBlockMin = std::chrono::milliseconds{100};

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

// Graphics stages in the order of the hash arrays (Maxwell's VertexA, VertexB, tessellation
// control, tessellation evaluation, geometry, fragment); only the stages a pipeline has.
std::string StageText(const std::array<u64, 6>& stages) {
    static constexpr std::array<const char*, 6> names{"vsA", "vs", "tcs", "tes", "gs", "fs"};
    std::string text;
    for (size_t i = 0; i < stages.size(); ++i) {
        if (stages[i] != 0) {
            text += fmt::format("{}{} {:016x}", text.empty() ? "" : " ", names[i], stages[i]);
        }
    }
    return text.empty() ? std::string{"(no stages)"} : text;
}

bool DrawTiming() {
    return DrawTimingRequested() && timestamp_host_reset && !slots.empty();
}

// Caller holds ring_mutex and has checked that there are slots.
u32 NextSlot() {
    const u32 slot = slot_next;
    slot_next = (slot_next + 1) % static_cast<u32>(slots.size());
    return slot;
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
        const u64 key = slot.hash ^ (slot.compute ? 0x9e3779b97f4a7c15ULL : 0);
        const auto [it, inserted] = index.try_emplace(key, times.size());
        if (inserted) {
            times.push_back({&slot});
        }
        Time& time = times[it->second];
        time.total_ms += ms;
        time.max_ms = std::max(time.max_ms, ms);
        ++time.count;
    }
    const bool draws = DrawTiming();
    if (timed == 0) {
        return draws ? "no timed dispatches or draws" : "no timed dispatches";
    }
    std::ranges::sort(times, [](const Time& a, const Time& b) { return a.total_ms > b.total_ms; });
    std::string text = fmt::format("{} {} took {:.1f} ms on the GPU, slowest first:", timed,
                                   draws ? "dispatches and draws" : "dispatches", all_ms);
    for (size_t i = 0; i < times.size() && i < 12; ++i) {
        const Time& time = times[i];
        if (time.slot->compute) {
            text += fmt::format(
                "\n    compute {:016x} {:.1f} ms (x{}, longest {:.1f} ms) grid {}x{}x{}",
                time.slot->hash, time.total_ms, time.count, time.max_ms, time.slot->grid_x,
                time.slot->grid_y, time.slot->grid_z);
        } else {
            text += fmt::format("\n    graphics {:.1f} ms (x{}, longest {:.1f} ms) {}",
                                time.total_ms, time.count, time.max_ms,
                                StageText(time.slot->stages));
        }
    }
    return text;
}

// The submissions of ticks (known_done - 1, tick], oldest first, with their GPU times.
std::string SubmitTimes(u64 tick, u64 known_done, std::chrono::steady_clock::time_point wait_start) {
    if (submit_pool == VK_NULL_HANDLE || timestamp_device == nullptr) {
        return {};
    }
    struct Row {
        u64 tick{};
        double submitted_ms{};
        std::array<u64, 8> data{}; // Four timestamps, each with its availability
        bool timed{};
    };
    std::vector<Row> rows;
    {
        std::scoped_lock lock{submit_mutex};
        for (u32 i = 0; i < submits.size(); ++i) {
            const Submit& submit = submits[i];
            if (!submit.valid || submit.tick + 1 < known_done || submit.tick > tick) {
                continue;
            }
            Row row{.tick = submit.tick,
                    .submitted_ms = std::chrono::duration<double, std::milli>(submit.submitted -
                                                                              wait_start)
                                        .count()};
            const VkResult result = timestamp_device->GetQueryResults(
                submit_pool, i * 4, 4, sizeof(row.data), row.data.data(), sizeof(u64) * 2,
                VK_QUERY_RESULT_64_BIT | VK_QUERY_RESULT_WITH_AVAILABILITY_BIT);
            row.timed = (result == VK_SUCCESS || result == VK_NOT_READY) && row.data[1] != 0 &&
                        row.data[3] != 0 && row.data[5] != 0 && row.data[7] != 0;
            rows.push_back(row);
        }
    }
    if (rows.empty()) {
        return {};
    }
    std::ranges::sort(rows, [](const Row& a, const Row& b) { return a.tick < b.tick; });
    const auto ms = [](u64 from, u64 to) {
        return static_cast<double>((to - from) & timestamp_mask) * timestamp_period_ns / 1e6;
    };
    std::string text = "submissions (submitted: ms after the wait began; idle: GPU time between "
                       "the previous submission's end and this one's start):";
    const Row* previous = nullptr;
    for (const Row& row : rows) {
        text += fmt::format("\n    tick {} submitted {:+.1f} ms", row.tick, row.submitted_ms);
        if (!row.timed) {
            text += ", GPU times not available";
        } else {
            if (previous != nullptr && previous->timed && previous->tick + 1 == row.tick) {
                text += fmt::format(", idle {:.1f} ms", ms(previous->data[6], row.data[0]));
            }
            text += fmt::format(", uploads {:.1f} ms, commands {:.1f} ms",
                                ms(row.data[0], row.data[2]), ms(row.data[4], row.data[6]));
        }
        previous = &row;
    }
    return text;
}

// The worker blocks that overlap [wait_start, wait_end], oldest first.
std::string WorkerTimes(std::chrono::steady_clock::time_point wait_start,
                        std::chrono::steady_clock::time_point wait_end) {
    std::vector<WorkerBlock> blocks;
    {
        std::scoped_lock lock{worker_mutex};
        for (const WorkerBlock& block : worker_blocks) {
            if (block.end > wait_start && block.start < wait_end &&
                block.end != std::chrono::steady_clock::time_point{}) {
                blocks.push_back(block);
            }
        }
    }
    if (blocks.empty()) {
        return {};
    }
    std::ranges::sort(blocks, [](const WorkerBlock& a, const WorkerBlock& b) {
        return a.start < b.start;
    });
    const auto ms = [](auto duration) {
        return std::chrono::duration<double, std::milli>(duration).count();
    };
    std::string text = "Vulkan worker during the wait (start: ms after the wait began):";
    for (const WorkerBlock& block : blocks) {
        if (block.shader_hash != 0) {
            text += fmt::format("\n    compute pipeline {:016x} build {:.1f} ms, start {:+.1f} ms",
                                block.shader_hash, ms(block.end - block.start),
                                ms(block.start - wait_start));
        } else {
            text += fmt::format("\n    command chunk {:.1f} ms, start {:+.1f} ms",
                                ms(block.end - block.start), ms(block.start - wait_start));
        }
    }
    return text;
}

void PushWorkerBlock(const WorkerBlock& block) {
    std::scoped_lock lock{worker_mutex};
    worker_blocks[worker_block_next] = block;
    worker_block_next = (worker_block_next + 1) % WorkerBlockCount;
}

} // Anonymous namespace

void NoteWorkerBuild(u64 shader_hash, std::chrono::steady_clock::time_point start,
                     std::chrono::steady_clock::time_point end) {
    if (!Enabled() || end - start < WorkerBlockMin) {
        return;
    }
    LOG_WARNING(Render_Vulkan,
                "Compute pipeline {:016x} kept the Vulkan worker waiting {:.0f} ms for its build",
                shader_hash, std::chrono::duration<double, std::milli>(end - start).count());
    PushWorkerBlock({.start = start, .end = end, .shader_hash = shader_hash});
}

void NoteWorkerChunk(std::chrono::steady_clock::time_point start,
                     std::chrono::steady_clock::time_point end) {
    if (!Enabled() || end - start < WorkerBlockMin) {
        return;
    }
    PushWorkerBlock({.start = start, .end = end});
}

void SetSubmitPool(VkQueryPool pool, u32 entries) {
    std::scoped_lock lock{submit_mutex};
    submit_pool = pool;
    submits.assign(pool != VK_NULL_HANDLE ? entries : 0, Submit{});
    submit_next = 0;
}

u32 BeginSubmit(vk::CommandBuffer cmdbuf, vk::CommandBuffer upload_cmdbuf) {
    if (!Enabled()) {
        return NoSlot;
    }
    u32 entry;
    {
        std::scoped_lock lock{submit_mutex};
        if (submits.empty()) {
            return NoSlot;
        }
        entry = submit_next;
        submit_next = (submit_next + 1) % static_cast<u32>(submits.size());
        submits[entry].valid = false;
    }
    // The upload buffer runs first in the same submission, so its reset comes before every write.
    upload_cmdbuf.ResetQueryPool(submit_pool, entry * 4, 4);
    upload_cmdbuf.WriteTimestamp(VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, submit_pool, entry * 4);
    cmdbuf.WriteTimestamp(VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, submit_pool, entry * 4 + 2);
    return entry;
}

void EndSubmit(u32 entry, vk::CommandBuffer cmdbuf, vk::CommandBuffer upload_cmdbuf) {
    if (entry == NoSlot) {
        return;
    }
    upload_cmdbuf.WriteTimestamp(VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, submit_pool, entry * 4 + 1);
    cmdbuf.WriteTimestamp(VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, submit_pool, entry * 4 + 3);
}

void Submitted(u32 entry, u64 tick) {
    if (entry == NoSlot) {
        return;
    }
    std::scoped_lock lock{submit_mutex};
    if (entry < submits.size()) {
        submits[entry] = {.tick = tick, .submitted = std::chrono::steady_clock::now(),
                          .valid = true};
    }
}

bool Enabled() {
    return ThresholdMs() != 0;
}

bool DrawTimingRequested() {
    static const bool value = [] {
        const char* env = std::getenv("EDEN_NX2_SLOW_WAIT_DRAWS");
        return env != nullptr && env[0] != '\0' && env[0] != '0';
    }();
    return value && Enabled();
}

void SetTimestampPool(const vk::Device* device, VkQueryPool pool, u32 slot_count, u32 valid_bits,
                      float period_ns, bool host_reset) {
    std::scoped_lock lock{ring_mutex};
    timestamp_device = device;
    timestamp_host_reset = host_reset;
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

u32 NoteGraphics(u64 tick, const std::array<u64, 6>& stage_hashes) {
    if (!Enabled()) {
        return NoSlot;
    }
    u64 pipeline_hash = 0xcbf29ce484222325ULL;
    for (const u64 stage : stage_hashes) {
        pipeline_hash = (pipeline_hash ^ stage) * 0x100000001b3ULL;
    }
    std::scoped_lock lock{ring_mutex};
    u32 slot = NoSlot;
    if (DrawTiming()) {
        // Reset here, on the host: the draw is recorded inside a render pass. The slot's previous
        // use is seconds old, long finished on the GPU.
        slot = NextSlot();
        slots[slot] = {.tick = tick, .hash = pipeline_hash, .stages = stage_hashes,
                       .compute = false};
        timestamp_device->ResetQueryPool(timestamp_pool, slot * 2, 2);
    }
    if (ring_used != 0) {
        Entry& last = Last();
        if (!last.compute && last.tick == tick && last.hash == pipeline_hash) {
            ++last.count;
            return slot;
        }
    }
    Push({.tick = tick, .hash = pipeline_hash, .stages = stage_hashes, .count = 1});
    return slot;
}

u32 NoteCompute(u64 tick, u64 shader_hash, u32 grid_x, u32 grid_y, u32 grid_z) {
    if (!Enabled()) {
        return NoSlot;
    }
    std::scoped_lock lock{ring_mutex};
    u32 slot = NoSlot;
    if (!slots.empty()) {
        slot = NextSlot();
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
                list += fmt::format("\n    graphics x{} {}", work[i].count,
                                    StageText(entry.stages));
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
        const auto wait_start = std::chrono::steady_clock::now() -
                                std::chrono::duration_cast<std::chrono::steady_clock::duration>(waited);
        if (const std::string submitted = SubmitTimes(tick, known_done, wait_start);
            !submitted.empty()) {
            LOG_WARNING(Render_Vulkan, "Slow GPU wait for tick {}: {}", tick, submitted);
        }
        if (const std::string worker = WorkerTimes(wait_start, std::chrono::steady_clock::now());
            !worker.empty()) {
            LOG_WARNING(Render_Vulkan, "Slow GPU wait for tick {}: {}", tick, worker);
        }
    }
}

} // namespace Vulkan::SlowWait
