// SPDX-FileCopyrightText: Copyright 2026 Satronho NX
// SPDX-License-Identifier: GPL-3.0-or-later

#include "satronho_adpf.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

#include <dirent.h>
#include <dlfcn.h>
#include <sys/types.h>

#include "common/common_types.h"
#include "common/logging.h"
#include "common/thread.h"
#include "core/core.h"
#include "core/perf_stats.h"

namespace Satronho::Adpf {
namespace {

// The NDK's APerformanceHint API (Android 13+; setThreads is Android 14+), looked up at run time.
struct APerformanceHintManager;
struct APerformanceHintSession;
using GetManagerFn = APerformanceHintManager* (*)();
using CreateSessionFn = APerformanceHintSession* (*)(APerformanceHintManager*, const int32_t*,
                                                     size_t, int64_t);
using UpdateTargetFn = int (*)(APerformanceHintSession*, int64_t);
using ReportActualFn = int (*)(APerformanceHintSession*, int64_t);
using CloseSessionFn = void (*)(APerformanceHintSession*);
using SetThreadsFn = int (*)(APerformanceHintSession*, const pid_t*, size_t);

struct Api {
    GetManagerFn get_manager{};
    CreateSessionFn create_session{};
    UpdateTargetFn update_target{};
    ReportActualFn report_actual{};
    CloseSessionFn close_session{};
    SetThreadsFn set_threads{};
    APerformanceHintManager* manager{};

    bool Usable() const {
        return manager != nullptr && create_session != nullptr && update_target != nullptr &&
               report_actual != nullptr && close_session != nullptr;
    }
};

const Api& GetApi() {
    static const Api api = [] {
        Api result;
        void* const library = dlopen("libandroid.so", RTLD_NOW | RTLD_LOCAL);
        if (library == nullptr) {
            return result;
        }
        result.get_manager =
            reinterpret_cast<GetManagerFn>(dlsym(library, "APerformanceHint_getManager"));
        result.create_session =
            reinterpret_cast<CreateSessionFn>(dlsym(library, "APerformanceHint_createSession"));
        result.update_target = reinterpret_cast<UpdateTargetFn>(
            dlsym(library, "APerformanceHint_updateTargetWorkDuration"));
        result.report_actual = reinterpret_cast<ReportActualFn>(
            dlsym(library, "APerformanceHint_reportActualWorkDuration"));
        result.close_session =
            reinterpret_cast<CloseSessionFn>(dlsym(library, "APerformanceHint_closeSession"));
        result.set_threads =
            reinterpret_cast<SetThreadsFn>(dlsym(library, "APerformanceHint_setThreads"));
        if (result.get_manager != nullptr) {
            result.manager = result.get_manager();
        }
        return result;
    }();
    return api;
}

// The threads whose speed sets the frame rate. Linux keeps 15 characters of a thread's name.
bool IsFrameThread(std::string_view name) {
    if (name.starts_with("CPUCore_") || name == "CPUThread") {
        return true;
    }
    static constexpr std::array<std::string_view, 4> exact{"GPU", "GPUBack", "VulkanWorker",
                                                           "VulkanPresent"};
    return std::ranges::find(exact, name) != exact.end();
}

std::vector<int32_t> FindFrameThreads() {
    std::vector<int32_t> tids;
    DIR* const tasks = opendir("/proc/self/task");
    if (tasks == nullptr) {
        return tids;
    }
    while (const dirent* const entry = readdir(tasks)) {
        if (entry->d_name[0] < '0' || entry->d_name[0] > '9') {
            continue;
        }
        std::ifstream comm{std::string{"/proc/self/task/"} + entry->d_name + "/comm"};
        std::string name;
        if (comm && std::getline(comm, name) && IsFrameThread(name)) {
            tids.push_back(static_cast<int32_t>(std::strtol(entry->d_name, nullptr, 10)));
        }
    }
    closedir(tasks);
    std::ranges::sort(tids);
    return tids;
}

using namespace std::chrono_literals;
constexpr int64_t Target30 = 33'333'333;
constexpr int64_t Target60 = 16'666'667;

std::atomic_bool enabled{true};
std::mutex mutex;
std::condition_variable wake;
std::thread worker;
bool stop_requested{};

void Run(Core::System& system) {
    Common::SetCurrentThreadName("SatronhoAdpf");
    const Api& api = GetApi();
    auto& perf_stats = system.GetPerfStats();
    perf_stats.SetFrameTimeHistoryEnabled(true);

    APerformanceHintSession* session = nullptr;
    std::vector<int32_t> tids;
    int64_t target = Target30;
    std::optional<Core::FrameTimeHistory::Clock::time_point> reported_until;
    auto next_thread_scan = std::chrono::steady_clock::now();
    u64 reports = 0;
    u64 late = 0;
    u32 sessions = 0;

    std::unique_lock lock{mutex};
    while (!wake.wait_for(lock, 100ms, [] { return stop_requested; })) {
        lock.unlock();
        const auto now = std::chrono::steady_clock::now();
        if (now >= next_thread_scan) {
            next_thread_scan = now + 2s;
            auto found = FindFrameThreads();
            if (!found.empty() && found != tids) {
                if (session != nullptr && api.set_threads != nullptr &&
                    api.set_threads(session, found.data(), found.size()) == 0) {
                    tids = std::move(found);
                } else {
                    if (session != nullptr) {
                        api.close_session(session);
                    }
                    session = api.create_session(api.manager, found.data(), found.size(), target);
                    if (session == nullptr) {
                        LOG_WARNING(Frontend, "Satronho ADPF: the phone refused a hint session");
                        lock.lock();
                        break;
                    }
                    ++sessions;
                    tids = std::move(found);
                }
                LOG_INFO(Frontend, "Satronho ADPF: {} threads in the hint session (target {} ms)",
                         tids.size(), target / 1'000'000);
            }
        }

        if (session != nullptr) {
            const auto frames = perf_stats.GetRecentFrameTimes(2s);
            u32 fast = 0;
            for (const auto& frame : frames) {
                if (frame.ms < 22.0f) {
                    ++fast;
                }
                if (reported_until && frame.end <= *reported_until) {
                    continue;
                }
                // A frame over half a second is a load or a pause, not a pace to chase.
                if (frame.ms <= 0.0f || frame.ms > 500.0f) {
                    continue;
                }
                const auto ns = static_cast<int64_t>(frame.ms * 1'000'000.0);
                api.report_actual(session, ns);
                ++reports;
                if (ns > target + target / 10) {
                    ++late;
                }
            }
            if (!frames.empty()) {
                reported_until = frames.back().end;
            }
            // A game that reaches 60 FPS for most of two seconds aims at 60 from then on; until
            // then, at 30, so a game capped at 30 does not keep the phone at full clock.
            if (target == Target30 && frames.size() >= 60 && fast * 4 >= frames.size() * 3) {
                target = Target60;
                api.update_target(session, target);
                LOG_INFO(Frontend, "Satronho ADPF: the game runs at 60, target 16 ms");
            }
        }
        lock.lock();
    }

    if (session != nullptr) {
        api.close_session(session);
    }
    LOG_INFO(Frontend,
             "Satronho ADPF summary: {} frames reported, {} over the target, {} sessions, "
             "target {} ms",
             reports, late, sessions, target / 1'000'000);
}

} // Anonymous namespace

bool IsSupported() {
    return GetApi().Usable();
}

void SetEnabled(bool value) {
    enabled = value;
}

bool IsEnabled() {
    return enabled;
}

void Start(Core::System& system) {
    Stop();
    if (!enabled) {
        LOG_INFO(Frontend, "Satronho ADPF: off");
        return;
    }
    if (!IsSupported()) {
        LOG_INFO(Frontend, "Satronho ADPF: this phone has no performance hints");
        return;
    }
    {
        std::scoped_lock lock{mutex};
        stop_requested = false;
    }
    worker = std::thread([&system] { Run(system); });
}

void Stop() {
    {
        std::scoped_lock lock{mutex};
        stop_requested = true;
    }
    wake.notify_all();
    if (worker.joinable()) {
        worker.join();
    }
}

} // namespace Satronho::Adpf
